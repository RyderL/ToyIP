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
	
	if (protocol == IPPROTO_UDP)
		sk->protocol = IP_UDP;
	else if (protocol == IPPROTO_TCP)
		sk->protocol = IP_TCP;
	else assert(0);

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
	int rc = 0;

	if (addr->sin_family == AF_UNSPEC) {
		//sk->ops->disconnect(sk, flags);
		sock->state = sk->err ? SS_DISCONNECTING : SS_UNCONNECTED;
		goto out;
	}

    switch (sock->state) {
    default:
        sk->err = -EINVAL;
        goto out;
    case SS_CONNECTED:
        sk->err = -EISCONN;
        goto out;
    case SS_CONNECTING:
        sk->err = -EALREADY;
        goto out;
    case SS_UNCONNECTED:
        sk->err = -EISCONN;
        if (sk->state != TCP_CLOSE) {
            goto out;
        }

        sk->ops->connect(sk, addr); /* ���������net_ops�еķ�����connnect */
        sock->state = SS_CONNECTING;
        sk->err = -EINPROGRESS;

		if (sock->flags & O_NONBLOCK) {
			goto out;
		}
		wait_sleep(&sock->sleep);

		switch (sk->err) {
		case -ETIMEDOUT:
		case -ECONNREFUSED:
			goto sock_error;
		}

		if (sk->err != 0) {
			goto out;
		}

		sock->state = SS_CONNECTED;  /* ���ӳɹ� */
		break;
	}
out:
	return sk->err;
sock_error:
	rc = sk->err;
	socket_free(sock);
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

/* inet_lookup ���ݶ˿ں�Ѱ�Ҷ�Ӧ��socket */
struct sock *
	inet_lookup(uint16_t sport, uint16_t dport)
{
	/* udp��tcp��Ҫ���ֶԴ� */
	struct socket * sock = socket_lookup(sport, dport);
	if (sock == NULL) return NULL;
	return sock->sk;
}

int
inet_close(struct socket *sock)
{
	struct sock *sk = sock->sk;
	int err = 0;

	if (!sock) {
		return 0;
	}

	if (err) {
		print_err("Error on socket closing\n");
		return -1;
	}

	pthread_mutex_lock(&sk->lock);
	sock->state = SS_DISCONNECTING;
	if (sock->sk->ops->close(sk) != 0) {
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

	if (sk->ops->set_sport) {
		if (err = sk->ops->set_sport(sk, bindaddr) < 0)	{	
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
		newsock->sk = newsk;
		/* ���Է��ĵ�ַ��Ϣ��¼���� */
		if (skaddr) {
			skaddr->sin_addr.s_addr = htonl(newsk->daddr);
			skaddr->sin_port = htons(newsk->dport);
		}
		err = 0;
	}
out:
	return err;
}
