#include "syshead.h"
#include "tcp.h"
#include "skbuff.h"
#include "sock.h"

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

static int
tcp_synrecv_ack(struct tcp_sock *tsk)
{
	if (tsk->parent->sk.state != TCP_LISTEN) return -1;
	tcp_accept_enqueue(tsk);
	wait_wakeup(&tsk->parent->wait);
	return 0;
}

/* tcp_clean_retransmission_queue��sk���ش����н�������,�������յ���ȷ�ϵ����ݶ��� */
static int
tcp_clean_retransmission_queue(struct sock *sk, uint32_t una)
{
	struct tcp_sock *tsk = tcp_sk(sk);
	struct sk_buff *skb;
	int rc = 0;

	pthread_mutex_lock(&sk->write_queue.lock);
	/* ��Ҫע�����,��write_queue�е��������ϸ��շ���˳�����е�. 
	 ����write_queue��skb��end_seq�������� */
	while ((skb = skb_peek(&sk->write_queue)) != NULL) {
		/* �ͷŵ��Ѿ����յ���ȷ�ϵ����� */
		if (skb->end_seq <= una) {
			skb_dequeue(&sk->write_queue);
			skb->refcnt--;
			free_skb(skb);
		}
		else {
			break;
		}
	}

	/* skb == NULL��ʾҪ���͵�����ȫ���������,���Ҷ����յ���ȷ��,Ҳ���Ƿ��ͳɹ� */
	if (skb == NULL) {
		tcp_stop_retransmission_timer(tsk);
	}

	pthread_mutex_unlock(&sk->write_queue.lock);
	return rc;
}

static void
tcp_reset(struct sock *sk) {
	switch (sk->state) {
	case TCP_SYN_SENT:
		sk->err = -ECONNREFUSED;	/* ����ʧ�� */
		break;
	/* �˶˽��յ��Զ˵ķ��͵�FIN,����CLOSE_WAIT״̬,��ʱ�Է���Ӧ���ٷ���tcp����,
	 ��Ϊ���͵�FIN��ʾ�ر�д�Ĺܵ�.���Ѿ��رյĹܵ�д����,�ᵼ�¹ܵ�����(EPIPE)����. */
	case TCP_CLOSE_WAIT: 
		sk->err = -EPIPE;			
		break;
	case TCP_CLOSE:
		return;
	default:
		sk->err = -ECONNRESET;
		break;
	}

	tcp_free_sock(sk);
}

/* tcp_drop ���ڶ������ݱ�. */
static inline int
tcp_drop(struct tcp_sock *tsk, struct sk_buff *skb)
{
	free_skb(skb);
	return 0;
}

static int
tcp_packet_filter(struct tcp_sock *tsk, struct tcphdr *th, struct sk_buff *skb)
{
	struct tcb *tcb = &tsk->tcb;

	if (skb->dlen > 0 && tcb->rcv_wnd == 0) return 0;
	/* ���յ��İ������к����С�������ڴ����յ���һ�����ݱ������к�(rcv_nxt),��ô����һ��
	�ش������ݰ�,ͬʱ,������кŴ���rcv_nxt+rcv_wnd,��ʾ�����ǶԷ�����̫��,��ȻҲ����
	�Ǳ��ԭ��.��֮��Щ�������õ����ݱ�. */
	if (th->seq < tcb->rcv_nxt ||
		th->seq > (tcb->rcv_nxt + tcb->rcv_wnd)) {
		tcp_sock_dbg("Received invalid segment", (&tsk->sk));
		return 0;
	}

	return 1;
}

static inline int 
tcp_discard(struct tcp_sock *tsk, struct sk_buff *skb, struct tcphdr *th)
{
	free_skb(skb);
	return 0;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

static struct tcp_sock *
tcp_listen_child_sock(struct tcp_sock *tsk, struct tcphdr *thr, struct iphdr * ih)
{
	struct sock *newsk = tcp_alloc_sock();
	struct tcp_sock *newtsk = tcp_sk(newsk);
	newsk->saddr = ih->daddr;
	newsk->daddr = ih->saddr;
	newsk->sport = thr->dport;
	newsk->dport = thr->sport;

	newtsk->parent = tsk;
	list_add(&newtsk->list, &tsk->listen_queue);	/* ���µ�sock����������� */
	return newtsk;
}

/* tcp_listen���ڼ��� */
static int 
tcp_handle_listen(struct tcp_sock *tsk, struct sk_buff *skb, struct tcphdr *th)
{
	/* tcp�涨,syn���Ķβ���Я������,����Ҫ���ĵ�һ����� */
	struct tcp_sock *newtsk;
	struct iphdr *iphdr = ip_hdr(skb);
	
	/* 1. ���rst */
	if (th->rst) goto discard;

	/* 2. ���ack */
	if (th->ack) {
		tcp_send_reset(tsk);
		goto discard;
	}

	/* 3. ���syn */
	if (!th->syn) goto discard;

	newtsk = tcp_listen_child_sock(tsk, th, iphdr);
	/* ������һ���µ�sock֮��,��Ҫ����sock��������� */
	if (!newtsk) goto discard;
	tcp_set_state((&newtsk->sk), TCP_SYN_RECEIVED);

	struct tcb *tc = &newtsk->tcb;
	/* ׼����Է�����ack�Լ�syn */
	tc->irs = th->seq;
	tc->isn = generate_isn();
	tc->snd_nxt = tc->isn;		/* ���͸��Զ˵�seq��� */
	tc->rcv_nxt = th->seq + 1;	/* ���͸��Զ˵�ack���, �Է����͵�syn���ĵ�һ����� */
	tcp_send_synack(&newtsk->sk);
	tcp_established_or_syn_recvd_socks_enqueue(&newtsk->sk);
	tc->snd_nxt = tc->isn + 1;	/* syn���ĵ�һ����� */
	tc->snd_una = tc->isn;
discard:
	free_skb(skb);
	return 0;
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

static int
tcp_synsent(struct tcp_sock *tsk, struct sk_buff *skb, struct tcphdr *th)
{
	struct tcb *tcb = &tsk->tcb;
	struct sock *sk = &tsk->sk;

	tcp_sock_dbg("state is synsent", sk);

	if (th->ack) {
		/* th->ack_seq < tcb->isn �Լ� th->ack_seq > tcb->snd_nxt
		   ����tcpЭ��ջ�Ļ�,���ǲ����ܵ�. */
		if (th->ack_seq < tcb->isn || th->ack_seq > tcb->snd_nxt) {
			tcp_sock_dbg("ACK is unacceptable", sk);
			if (th->rst) goto discard;
			goto reset_and_discard;
		}
		
		if (th->ack_seq < tcb->snd_una || th->ack_seq > tcb->snd_nxt) {
			tcp_sock_dbg("ACK is unacceptable", sk);
			goto reset_and_discard;
		}
	}

	/* ���Ǹ��Է�������һ��syn,��ͼ���ӶԷ�,Ȼ��Է�������һ��rst */
	if (th->rst) { 
		// tofix: ���յ�rst,Ӧ�öϿ�����
		goto discard;
	}

	if (!th->syn) goto discard;

	tcb->rcv_nxt = th->seq + 1;  /* tcb->rcv_nxt��ʾ�ڴ����յ������ */
	tcb->irs = th->seq;			 /* tcb->irs��ʾ���ݷ��͵ĳ�ʼ���к�(initial receive sequence number) */

	if (th->ack) {  /* �Է�ȷ����syn */
		tcb->snd_una = th->ack_seq; /* una��ʾ��δȷ�ϵ����к� */
		/* ���Խ�syn���ݱ���write queue���Ƴ��� */
		tcp_clean_retransmission_queue(sk, tcb->snd_una);
	}

	/* һ����˵,δ��ȷ�ϵ����кŲ���С��isn */
	if (tcb->snd_una > tcb->isn) { /* ��Ϊ�ͻ���,���յ��˷���˷��͵�syn, ack */
		tcp_set_state(sk, TCP_ESTABLISHED); /* ���ӽ����ɹ� */
		tcb->snd_una = tcb->snd_nxt; /* snd_nxt��ʾ��������ʱ��һ��Ҫ���õ����к� */
		tcp_send_ack(&tsk->sk);  /* ����ack,��3������ */
		wait_wakeup(&tsk->wait);
	}
	else {
		/* �����Ӧ��һ�ָ�����С���¼�,�Ǿ�������ͬʱ��ͼ���ӶԷ�,Ҳ����ͬʱ�򿪵����.
		 �������˶�����syn_received״̬,һ���Է����յ����͵�syn��ack,ֱ�ӽ���established
		 ״̬.*/
		tcp_set_state(sk, TCP_SYN_RECEIVED);
		tcb->snd_una = tcb->isn;
		tcp_send_synack(&tsk->sk); /* ��2������ */
	}

discard:
	tcp_drop(tsk, skb);
    return 0;
reset_and_discard:
	/* todo: reset */
	tcp_drop(tsk, skb);
	return 0;
}

static int 
tcp_closed(struct tcp_sock *tsk, struct sk_buff *skb, struct tcphdr *th)
{
	/* ����incoming segment(���͹��������ݱ�)�е����ݶ��ᱻ������.������ݱ�����
	   rst, ֱ�Ӷ���,���������,����Ҫ����һ����rst�����ݱ���Ϊ��Ӧ. */
	int rc = -1;
	
	tcp_sock_dbg("state is closed", (&tsk->sk));

	if (th->rst) {  
		tcp_discard(tsk, skb, th);
		rc = 0;
		goto out;
	}

	/* todo */
	if (th->ack) {

	}
	else {

	}

	rc = tcp_send_reset(tsk);
	free_skb(skb);
out:
	return rc;
}

int
tcp_process(struct sock *sk, struct tcphdr *th, struct sk_buff *skb)
{
	struct tcp_sock *tsk = tcp_sk(sk); 
	struct tcb *tcb = &tsk->tcb; /* transmission control block ������ƿ� */

	tcp_sock_dbg("input state", sk);

	switch (sk->state) {
	case TCP_CLOSE:   /* ����close״̬,���յ���tcp���ݱ� */
		return tcp_closed(tsk, skb, th);
	case TCP_LISTEN:  /* ����listen״̬ */
		return tcp_handle_listen(tsk, skb, th);
	case TCP_SYN_SENT: /* �Ѿ�����������һ��syn */
		return tcp_synsent(tsk, skb, th);
	}

	/* 1.���sequence number, tcp_packet_filter�ǵ�һ�������,Я����tcp���ݵİ�
	 �ܹ�ͨ��,����ı���syn,fin�Ȳ�Я�����ݵİ�,����,Ҫ���˵��ش��Ĳ���.
	 */
	if (!tcp_packet_filter(tsk, th, skb)) {
		if (!th->rst) {
			tcp_send_ack(sk);	/* ���߶Է�,�ҽ��յ���������ݰ� */
		}
		return tcp_drop(tsk, skb);
	}

	/* 2.���rst bit */
	

	/* 3.��鰲ȫ�Ժ����ȼ� */
	
	/* 4.���syn */
	if (th->syn) {
		/* ��listen��syn_sent����״̬���Խ���syn,����ʵ��,������״̬�������Ѿ��������,
		 �������е�����,��ʾskһ��������������״̬,����,����Ҫע��һ��:
		 �ش���syn���ݰ���ǰ���Ѿ���������.����������,������ݰ��Ǵ����. */
		tcp_send_reset(tsk);
		if (sk->state == TCP_SYN_RECEIVED && tsk->parent) {
			/*��ʱtskһ��������tcp_established_or_syn_recvd_socks������,����Ҫ��
			 ������ɾ����sock */
			tcp_established_or_syn_recvd_socks_remove(sk);
			tcp_free_sock(sk);
		}
	}

	/* 5.���ack */
	if (!th->ack) return tcp_drop(tsk, skb);

	/* ���е�������,���յ�������syn,��ack
	   ����ʲô�����? �������������,�������ӵ������������ؽ�������. */
	switch (sk->state) {
	case TCP_SYN_RECEIVED:
		/* ��Ϊ�����,���յ��˶Է����͵�ack,���ӳɹ����� */
		if (tcb->snd_una <= th->ack_seq && th->ack_seq <= tcb->snd_nxt) {
			if (tcp_synrecv_ack(tsk) < 0) {
				return tcp_drop(tsk, skb);
			}
			tcb->snd_una = th->ack_seq;
			tcp_set_state(sk, TCP_ESTABLISHED);	
		}
		else {
			tcp_send_reset(tsk);
			return tcp_drop(tsk, skb);
		}
		break;
	case TCP_ESTABLISHED:
	case TCP_FIN_WAIT_1:	/* �����ر� */
	case TCP_FIN_WAIT_2:	/* fin_wait_2״̬���ɿ��Խ��նԷ����͵�����,ֱ���Է�������fin,Ȼ�����time_wait״̬ */
	case TCP_CLOSE_WAIT:	/* ���յ���FIN,ִ�б����ر� */
	case TCP_CLOSING:		/* ����ͬʱ�ر�,�����closing״̬ */
	case TCP_LAST_ACK:
		/* ����������ж������Ƕ�tcp���ݰ��ĵڶ��ι���,������Ҫ�ǹ��˵�Я��tcp���ݵ��ش�
		 ����,���˵����кŲ�������tcp���ݰ� */

		/* ������ȷ���Է���������ack_seq�Ƕ����Ƿ����Է��Ĳ��һ�û�н��յ�ȷ�ϵ����ݵ�ȷ�� */
		if (tcb->snd_una < th->ack_seq && th->ack_seq <= tcb->snd_nxt) {
			/* �����ʾ�Է��Ѿ��յ������ǵ����ݰ�,ack_seq���´�������˳���,һ�����յ�ack
			 ��ʾack_seq���֮ǰ�����ݶ��Ѿ��յ���. */
            tcb->snd_una = th->ack_seq;
			tcp_clean_retransmission_queue(sk, tcb->snd_una); 
		}
		
		if (th->ack_seq > tcb->snd_nxt) return tcp_drop(tsk, skb);
		if (th->ack_seq < tcb->snd_una) return tcp_drop(tsk, skb);
		/* ack_seq < snd_una ����ǳ����˶��Ѿ��������ݵĶ���ȷ��,ֱ�Ӷ�������,
		   ack_seq > snd_nxt ������ϲ����� */
		break;
	}
    
    /* 6.���URG bit */
	


	/* 7. segment text */
	pthread_mutex_lock(&sk->receive_queue.lock);
	switch (sk->state) {
	case TCP_ESTABLISHED:
	case TCP_FIN_WAIT_1:
	case TCP_FIN_WAIT_2:
		if (skb->dlen > 0) {	/* �����ݴ��ݹ��� */
			tcp_data_queue(tsk, th, skb);
			tsk->sk.ops->recv_notify(&tsk->sk);	/* �����ϲ����ڵȴ����ݵĽ��� */
		}
		break;
	case TCP_CLOSE_WAIT:
	case TCP_CLOSING:
	case TCP_LAST_ACK:
	case TCP_TIME_WAIT:
		/* close_wait, closing, last_ack, time_wait�⼸��״̬����һ����ͬ��,�Ǿ���
		 �����Ѿ����յ��˶Է����͵�fin,����ζ�ŶԷ����������ٷ�������(tcp����)����,�������
		 ��,������ȫ���Լ���.*/
		break;
	}

	/* 8, ���fin */
	 /* ��2�������Ǳ�֤,��fin֮ǰ������ȫ�����ճɹ���. */
	if (th->fin && (tcb->rcv_nxt - skb->dlen) == skb->seq) {
		tcp_sock_dbg("Received in-sequence FIN", sk);
        switch (sk->state) {
        case TCP_CLOSE:
        case TCP_LISTEN:
        case TCP_SYN_SENT:
            goto drop_and_unlock;
        }

		tcb->rcv_nxt += 1;	/* fin��Ҫ���ĵ�һ����� */
		tsk->flags |= TCP_FIN;
		tcp_send_ack(sk);
		tsk->sk.ops->recv_notify(&tsk->sk);

		switch (sk->state) {
		case TCP_SYN_RECEIVED:
		case TCP_ESTABLISHED:  /* close_wait �����ر� */
			tcp_set_state(sk, TCP_CLOSE_WAIT);
			tsk->sk.ops->recv_notify(&tsk->sk);
			break;
		case TCP_FIN_WAIT_1:
			/* ����ͬʱ����fin,����closing״̬ */
			tcp_set_state(sk, TCP_CLOSING);
			break;
		case TCP_FIN_WAIT_2: /* fin_wait_2���յ�fin֮��,����time_wait״̬,
							 ������һ��tcp���Ӿ������. */
			tcp_enter_time_wait(sk);
			break;
		case TCP_CLOSE_WAIT:
		case TCP_CLOSING:
		case TCP_LAST_ACK:
		case TCP_TIME_WAIT:
			break;
		}

	}
	free_skb(skb);
unlock:
	pthread_mutex_unlock(&sk->receive_queue.lock);
	return 0;
drop_and_unlock:
	tcp_drop(tsk, skb);
	goto unlock;
}

int
tcp_receive(struct tcp_sock *tsk, void *buf, int len)
{
	int rlen = 0;		/* rlen��ʾ�Ѿ����������� */
	int curlen = 0;
	struct sock *sk = &tsk->sk;
	struct socket *sock = sk->sock;
	memset(buf, 0, len);
	
	/* ����tcp���ݱ���ԭ������,������buf����,�����Ѿ�������FIN */
	while (rlen < len) {
		curlen = tcp_data_dequeue(tsk, buf + rlen, len - rlen);
		rlen += curlen;

		if (tsk->flags & TCP_PSH) {
			tsk->flags &= ~TCP_PSH;
			break;
		}

		/* ��ȡ���˽�β */
		if (tsk->flags & TCP_FIN || rlen == len) break;

		wait_sleep(&sk->recv_wait);
	}
	return rlen;
}

