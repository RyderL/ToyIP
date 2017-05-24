#include "syshead.h"
#include "tcp.h"
#include "list.h"

/**\
 * tcp_data_insert_ordered �������кŵ�˳����.
\**/
static void
tcp_data_insert_ordered(struct sk_buff_head *queue, struct sk_buff *skb)
{
	struct sk_buff *next;
	struct list_head *item, *tmp;

	list_for_each_safe(item, tmp, &queue->head) {
        next = list_entry(item, struct sk_buff, list);
		if (skb->seq < next->seq) {
			if (skb->end_seq > next->seq) {
				print_err("Could not join skbs\n");
			}
			else {
				skb->refcnt++;
				skb_queue_add(queue, skb, next);
				return;
			}
		}
		else if (skb->seq == next->seq) {
			/* ������ݱ��Ѿ����� */
			return;
		}
	}
	skb->refcnt++;
	skb_queue_tail(queue, skb);
}

static void
tcp_consume_ofo_queue(struct tcp_sock *tsk)
{
	struct sock *sk = &tsk->sk;
	struct tcb *tcb = &tsk->tcb;
	struct sk_buff *skb = NULL;

	while ((skb = skb_peek(&tsk->ofo_queue)) != NULL &&
		tcb->rcv_nxt == skb->seq) {
		tcb->rcv_nxt += skb->dlen;
		skb_dequeue(&tsk->ofo_queue);			 /* ���϶������������Ԫ�� */
		skb_queue_tail(&sk->receive_queue, skb); /* ��ӵ�β�� */
	}
}

/**\
 * tcp_data_dequeue ��������е����ݳ�����.
\**/
int
tcp_data_dequeue(struct tcp_sock *tsk, void *user_buf, int userlen)
{
	struct sock *sk = &tsk->sk;
	struct tcphdr *th;
	struct sk_buff *skb;
	int rlen = 0;
	int dlen;

	pthread_mutex_lock(&sk->receive_queue.lock);	/* ���ܶ��м��� */
	while (!skb_queue_empty(&sk->receive_queue) &&
		rlen < userlen) {
		skb = skb_peek(&sk->receive_queue);
		if (skb == NULL) break;
		th = tcp_hdr(skb);
		// tofix: tcpͷ�����ܴ��ڿ�ѡ��,���,ֱ�Ӵ�skb->payload��ʼ�������ܴ�������.
		// ��Ȼ,������ݶ�ȡ��ʱ��,�Ѿ���payloadָ�������ݲ���,�Ǿ�û��������. 
		dlen = (rlen + skb->dlen) > userlen ? (userlen - rlen) : skb->dlen;
		memcpy(user_buf, skb->payload, dlen);

		skb->dlen -= dlen;
		skb->payload += dlen;
		rlen += dlen;
		user_buf += dlen;

		if (skb->dlen == 0) { /* ��skb�������Ѿ�ȫ����ȡ�� */
			if (th->psh) tsk->flags |= TCP_PSH;
			skb_dequeue(&sk->receive_queue);
			skb->refcnt--;
			free_skb(skb);
		}
	}

	pthread_mutex_unlock(&sk->receive_queue.lock);
	return rlen;
}

/**\
 * tcp_data_queue ��������е����������. 
\**/
int
tcp_data_queue(struct tcp_sock *tsk, struct tcphdr *th, struct sk_buff *skb)
{
	struct sock *sk = &tsk->sk;
	struct tcb *tcb = &tsk->tcb;
	int rc = 0;

	if (!tcb->rcv_wnd) {	/* ���ܴ���Ϊ0�Ļ�,�������� */
		free_skb(skb);
		return -1;
	}

	int expected = skb->seq == tcb->rcv_nxt; 

	if (expected) { /* expected��ʾtcp���ݱ��ǰ��򵽴�� */
		tcb->rcv_nxt += skb->dlen; /* dlen���ȵ����ݱ��ɹ�ȷ�� */

		skb->refcnt++;
		skb_queue_tail(&sk->receive_queue, skb);  /* ��ӵ�β�� */
		tcp_consume_ofo_queue(tsk);
		tcp_stop_delack_timer(tsk);

		if (th->psh || (skb->dlen == tsk->rmss && ++tsk->delacks > 1)) {
			tsk->delacks = 0;
			tcp_send_ack(sk);
		}
		else {
			tsk->delack = timer_add(200, &tcp_send_delack, &tsk->sk);
		}
	}
	else {
		tcp_data_insert_ordered(&tsk->ofo_queue, skb);
		tcp_send_ack(sk);
	}
	return rc;
}