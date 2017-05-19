#include "syshead.h"
#include "inet.h"
#include "socket.h"
#include "sock.h"
#include "tcp.h"
#include "wait.h"
#include "netdev.h"

//
// inet ����ָ����tcp socket.
// 
extern struct net_ops tcp_ops;
extern struct net_ops udp_ops;


static int INET_OPS = 2;

struct net_family inet = {
	.create = inet_create,
};

static struct sock_ops sock_ops = {
	.connect = &inet_connect,
	.write = &inet_write,
	.bind = &inet_bind,
	.read = &inet_read,
	.listen = &inet_listen,
	.close = &inet_close,
	.free = &inet_free,
	.accept = &inet_accept,
};

static struct sock_type inet_ops[] = {
	{ 
	  .sock_ops = &sock_ops, .net_ops = &tcp_ops,
	  .type = SOCK_STREAM, .protocol = IPPROTO_TCP,
	},
	{
	  .sock_ops = &sock_ops,.net_ops = &udp_ops,
	  .type = SOCK_DGRAM, .protocol = IPPROTO_UDP,
	}
};

/* inet_create��Ҫ���ڸ�struct socket *sock�����ͳ�ʼ��struct sock *sk. */
int
inet_create(struct socket *sock, int protocol) 
{
	struct sock *sk;
	struct sock_type *skt = NULL;
	/* ����ֻ֧��udp����tcp */
	for (int i = 0; i < INET_OPS; i++) {
		if (inet_ops[i].type & sock->type) {
			skt = &inet_ops[i];
			break;
		}
	}

	if (!skt) {
		print_err("Could not find socktype for socket\n");
		return 1;
	}

	sock->ops = skt->sock_ops;	/* ��¼�¶�struct socket�Ĳ��ݷ���,�����Э��ջ��,
								sock->ops == &sock_opsʼ�ճ��� */
	sk = sk_alloc(skt->net_ops, protocol);	/* ����sock */
	
	
	sk->protocol = protocol;

	sock_init_data(sock, sk);	/* ��sock����������һЩ��ʼ������. */
	return 0;
}

int
inet_socket(struct socket *sock, int protocol)
{
	return 0;
}

int
inet_connect(struct socket *sock, const struct sockaddr_in *addr)
{ 
	struct sock *sk = sock->sk;
	int rc = -1;
	/* ������������ */
	if (sk->sport) goto out;
	if (sk->ops->connect)
		rc = sk->ops->connect(sk, addr);
out:
	return rc;
}

/*
 * ��������inet_write, inet_read�Ⱥ���ֱ�ӵ���sock��write, read����.
 */

int
inet_write(struct socket *sock, const void *buf, int len)
{
	struct sock *sk = sock->sk;
	return sk->ops->send_buf(sk, buf, len);
}

int
inet_read(struct socket *sock, void *buf, int len)
{
	struct sock *sk = sock->sk;
	return sk->ops->recv_buf(sk, buf, len);
}

int
inet_close(struct socket *sock)
{
	struct sock *sk = sock->sk;
	int err = 0;

	pthread_mutex_lock(&sk->lock);
	if (sock->sk->ops->close(sk) != 0) {	/* ���ȹر����� */
		print_err("Error on sock op close\n");
	}

	err = sk->err;
	pthread_mutex_unlock(&sk->lock);
	return err;
}

int
inet_free(struct socket *sock)
{
	struct sock *sk = sock->sk;
	sock_free(sk);
	free(sock->sk);
	return 0;
}

int
inet_bind(struct socket *sock, struct sockaddr_in * skaddr)
{
	struct sock *sk = sock->sk;	/* struct sock��ʾһ������ */
	int err = -1;
	uint32_t bindaddr;
	uint16_t bindport;

	if (sk->ops->bind)
		return sk->ops->bind(sock->sk, skaddr);

	if (sk->sport) goto err_out;
	/* ��������Ҫ���skadddr�еĵ�ַ�ǲ��Ǳ�����ַ */
	bindaddr = ntohl(skaddr->sin_addr.s_addr);
	bindport = ntohs(skaddr->sin_port);
	if (!local_ipaddress(bindaddr)) goto err_out;
	sk->saddr = bindaddr;

	if (sk->ops->set_sport) {
		if (err = sk->ops->set_sport(sk, bindport) < 0)	{	
			/* �趨�˿ڳ���,�����Ƕ˿��Ѿ���ռ�� */
			sk->saddr = 0;
			goto err_out;
		}
	}
	else {
		sk->sport = bindport;
	}
	/* �󶨳ɹ� */
	err = 0;
	sk->dport = 0;
	sk->daddr = 0;
err_out:
	return err;
}

int
inet_listen(struct socket *sock, int backlog)
{
	struct sock *sk = sock->sk;
	int err = -1;

	if (sock->type != SOCK_STREAM)
		return -1;

	err = sk->ops->listen(sk, backlog);
	return err;
}

/* inet_accept�������ڼ����Զ˷��͹��������� */
int
inet_accept(struct socket *sock, struct socket *newsock, 
	struct sockaddr_in* skaddr)
{
	struct sock *sk = sock->sk;
	struct sock *newsk;
	int err = -1;

	if (!sk) goto out;

	newsk = sk->ops->accept(sk);
	if (newsk) {
		/* ���Է��ĵ�ַ��Ϣ��¼���� */
		if (skaddr) {
			skaddr->sin_addr.s_addr = htonl(newsk->daddr);
			skaddr->sin_port = htons(newsk->dport);
		}
		sock_init_data(newsock, newsk);	/* ��struct sock��������ʼ������ */
		err = 0;
	}
out:
	return err;
}
