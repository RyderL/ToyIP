#include "syshead.h"
#include "inet.h"
#include "ip.h"
#include "sock.h"
#include "socket.h"
#include "utils.h"
#include "timer.h"
#include "wait.h"
#include "tcp.h"

#ifdef DEBUG_TCP
const char *tcp_dbg_states[] = {
	"TCP_LISTEN", "TCP_SYNSENT", "TCP_SYN_RECEIVED", "TCP_ESTABLISHED", "TCP_FIN_WAIT_1",
	"TCP_FIN_WAIT_2", "TCP_CLOSE", "TCP_CLOSE_WAIT", "TCP_CLOSING", "TCP_LAST_ACK", "TCP_TIME_WAIT",
};
#endif


/**\
 * tcp_init_segment �������ֽ���ĸ���ȫ��ת��Ϊ�����ֽ���.
\**/
static void
tcp_init_segment(struct tcphdr *th, struct iphdr *ih, struct sk_buff *skb) 
{
	/* ��Ҫ˵��һ�µ���,���ﲻ��Ҫת��ipͷ�����ֽ���,��Ϊ֮ǰ�Ѿ�ת������,ÿһ���ÿһ��
	 ������,����ҪԽ��,����Э��ջ����һ����ɫ. */
	th->sport = ntohs(th->sport);		/* 16λԴ�˿ں�   */
	th->dport = ntohs(th->dport);		/* 16λĿ�Ķ˿ں� */
	th->seq = ntohl(th->seq);			/* 32λ���к�		*/
	th->ack_seq = ntohl(th->ack_seq);	/* 32λȷ�����к� */
	th->win = ntohs(th->win);			/* 16λ���ڴ�С   */
	th->csum = ntohs(th->csum);			/* У���		    */
	th->urp = ntohs(th->urp);			/* 16λ����ָ��   */

	/* skb��ȫ��������ȫ�����������ֽ��� */
	skb->seq = th->seq;					/* �����ݱ���ʼ�����к� */
	skb->dlen = ip_len(ih) - tcp_hlen(th);	/* ʵ�����ݵĴ�С */
	skb->len = skb->dlen + th->syn + th->fin; 
	skb->end_seq = skb->seq + skb->dlen; /* �����ݱ���ֹ�����к� */
	skb->payload = th->data;
}

static void
tcp_clear_queues(struct tcp_sock *tsk) 
{
	pthread_mutex_lock(&tsk->ofo_queue.lock);
	skb_queue_free(&tsk->ofo_queue);
	pthread_mutex_unlock(&tsk->ofo_queue.lock);
}

void
tcp_in(struct sk_buff *skb)
{
	struct sock *sk;
	struct iphdr *iph;
	struct tcphdr *th;

	iph = ip_hdr(skb);		 
	th = (struct tcphdr *)iph->data; 

	tcp_init_segment(th, iph, skb);

	/* ����Ѱ�ҵ�sk��������һ��tcp_sock���� */
	sk = tcp_lookup_sock(iph->saddr, th->sport, iph->daddr, th->dport);

	if (sk == NULL) {
		print_err("No TCP socket for sport %d dport %d\n",
			th->sport, th->dport);
		free_skb(skb);
		return;
	}

	tcp_in_dbg(th, sk, skb);
	tcp_process(sk, th, skb);
}


int
tcp_checksum(struct sk_buff *skb, uint32_t saddr, uint32_t daddr)
{
	return tcp_udp_checksum(saddr, daddr, IP_TCP, skb->data, skb->len);
}


inline void 
__tcp_set_state(struct sock *sk, uint32_t state)
{
	sk->state = state;
}

/**\
 * generate_port ��������ӿ�.
\**/
uint16_t 
tcp_generate_port()
{
	// todo: ���õİ취������port
	static int port = 40000;
	return ++port + (timer_get_tick() % 10000);
}

int
tcp_generate_isn()
{
	// todo: ���õķ���������isn
	return (int)time(NULL) *rand();
}




int
tcp_done(struct sock *sk)
{
	tcp_established_or_syn_recvd_socks_remove(sk);
	tcp_free_sock(sk);
	if (sk->sock) {
		free_socket(sk->sock);
	}
	return 0;
}

void
tcp_clear_timers(struct sock *sk)
{
	struct tcp_sock *tsk = tcp_sk(sk);
	pthread_mutex_lock(&sk->write_queue.lock);
	tcp_stop_retransmission_timer(tsk);
	tcp_stop_delack_timer(tsk);
	pthread_mutex_unlock(&sk->write_queue.lock);
	timer_cancel(tsk->keepalive);
}

void 
tcp_stop_retransmission_timer(struct tcp_sock *tsk)
{
	if (tsk) {
		timer_cancel(tsk->retransmit);
		tsk->retransmit = NULL;
	}
}

void
tcp_release_retransmission_timer(struct tcp_sock *tsk)
{
	if (tsk) {
		timer_release(tsk->retransmit);
		tsk->retransmit = NULL;
	}
}

void
tcp_stop_delack_timer(struct tcp_sock *tsk)
{
	timer_cancel(tsk->delack);
	tsk->delack = NULL;
}

void
tcp_release_delack_timer(struct tcp_sock *tsk)
{
	timer_release(tsk->delack);
	tsk->delack = NULL;
}

void
tcp_handle_fin_state(struct sock *sk)
{
	switch (sk->state)
	{
		case TCP_CLOSE_WAIT:
			tcp_set_state(sk, TCP_LAST_ACK);
			break;
		case TCP_ESTABLISHED:
			tcp_set_state(sk, TCP_FIN_WAIT_1);
			break;
	default:
		break;
	}
}

static void
tcp_linger(uint32_t ts, void *arg)
{
	struct sock *sk = (struct sock *)arg;
	struct tcp_sock *tsk = tcp_sk(sk);
	timer_release(tsk->linger);		/* �ͷŶ�ʱ�� */
	tsk->linger = NULL;
	tcp_done(sk);		/* ���׽���������� */
}

void
tcp_enter_time_wait(struct sock *sk)
{
	/* ����TIME_WAIT״̬ */
	struct tcp_sock *tsk = tcp_sk(sk);
	tcp_set_state(sk, TCP_TIME_WAIT);
	tcp_clear_timers(sk);
	timer_cancel(tsk->linger);
	tsk->linger = timer_add(3000, &tcp_linger, sk);
}

