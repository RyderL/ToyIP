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

void
sk_init(struct sock *sk)
{
	sk->sock = NULL;
	wait_init(&sk->recv_wait);
	skb_queue_init(&sk->receive_queue);		/* ��ʼ�����ն��� */
	skb_queue_init(&sk->write_queue);		/* ��ʼ�����Ͷ��� */
	pthread_mutex_init(&sk->lock, NULL);	/* ��ʼ���� */
	sk->ops->init(sk);						/* net_ops����ʼ������ */
}

/* sk_init_with_socket���ڳ�ʼ��sk,���ҽ�sk��¼��sock�� */
void
sk_init_with_socket(struct socket *sock, struct sock *sk)
{
	sk_init(sk);
	sock->sk = sk;
	sk->sock = sock;
}

void 
sk_free(struct sock *sk)
{
	skb_queue_free(&sk->receive_queue);
	skb_queue_free(&sk->write_queue);
	pthread_mutex_destroy(&sk->lock);
}