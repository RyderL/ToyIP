#ifndef SOCK_H
#define SOCK_H

#include "socket.h"
#include "wait.h"
#include "skbuff.h"
#include <bits/pthreadtypes.h>

struct sock;

/* net_pos �൱�ڽӿ�,��װ��һ���������ķ��� */
struct net_ops {
	struct sock* (*alloc_sock)(int protocol);
	int(*init)(struct sock *sk);
	int(*connect)(struct sock *sk, const struct sockaddr *addr, int addr_len, int flags);
	int(*disconnect)(struct sock *sk, int flags);
	int(*write)(struct sock *sk, const void *buf, int len);
	int(*read)(struct sock *sk, void *buf, int len);
	int(*recv_notify)(struct sock *sk);
	int(*close)(struct sock *sk);
	int(*abort)(struct sock *sk);
};

/* sock
   ��Ҫ˵��һ�µ���,�ڴ��������,sport,dport,sadddr�Լ�daddr�洢�Ķ��������ֽ��� */
struct sock {
	struct socket *sock;				// 
	struct net_ops *ops;				// ��������ķ���
	struct wait_lock recv_wait;
	struct sk_buff_head receive_queue;	// ���ն���
	struct sk_buff_head write_queue;	// ���Ͷ���
	pthread_mutex_t lock;				// ���߳�����Ҫ����
	int protocol;						// Э��
	int state;
	int err;
	short int poll_events;				// 
	uint16_t sport;				
	uint16_t dport;						// �Է��˿ں�
	uint32_t saddr;						// Դip
	uint32_t daddr;						// �Զ�ip
};

static inline struct sk_buff*
write_queue_head(struct sock *sk) {
	return skb_peek(&sk->write_queue);
}

struct sock *sk_alloc(struct net_ops *ops, int protocol);
void sock_free(struct sock *sk);
void sock_init_data(struct socket *sock, struct sock *sk);
void sock_connected(struct sock *sk);

#endif // !SOCK_H