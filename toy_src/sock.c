#include "syshead.h"
#include "sock.h"
#include "socket.h"

struct sock *
sk_alloc(struct net_ops *ops, int protocol) 
{
	struct sock *sk;
	sk = ops->alloc_sock(protocol);
	sk->ops = ops;	/* ��¼��һ�ײ������� */
	return sk;
}

/* sock_init_data���ڳ�ʼ��sk,���ҽ�sk��¼��sock�� */
void
sock_init_data(struct socket *sock, struct sock *sk)
{
	sock->sk = sk;
	sk->sock = sock;

	wait_init(&sk->recv_wait);
	skb_queue_init(&sk->receive_queue);		/* ��ʼ�����ն��� */
	skb_queue_init(&sk->write_queue);		/* ��ʼ�����Ͷ��� */
	pthread_mutex_init(&sk->lock, NULL);	/* ��ʼ���� */

	sk->ops->init(sk);						/* net_ops����ʼ������ */
}

void 
sock_free(struct sock *sk)
{
	skb_queue_free(&sk->receive_queue);
	skb_queue_free(&sk->write_queue);
	pthread_mutex_destroy(&sk->lock);
}

