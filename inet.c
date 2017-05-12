#include "syshead.h"
#include "inet.h"
#include "socket.h"
#include "sock.h"
#include "tcp.h"
#include "wait.h"

//
// inet ����ָ����tcp socket
// ����˵��һ����,��������ǲ��ǲ�������.���뾫���һЩ����.
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
	.read = &inet_read,
	.close = &inet_close,
	.free = &inet_free,
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

int
inet_create(struct socket *sock, int protocol) 
{
	struct sock *sk;
	struct sock_type *skt = NULL;
	/* ����ֻ֧��UDP����TCP */
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

	sock->ops = skt->sock_ops;	
	sk = sk_alloc(skt->net_ops, protocol);	/* ����sock */
	
	if (protocol == IPPROTO_UDP)
		sk->protocol = IP_UDP;
	else if (protocol == IPPROTO_TCP)
		sk->protocol = IP_TCP;
	else assert(0);

	sock_init_data(sock, sk);
	return 0;
}

int
inet_socket(struct socket *sock, int protocol)
{
	return 0;
}

static int
inet_connect(struct socket *sock, const struct sockaddr *addr, int addr_len, int flags)
{ 
	struct sock *sk = sock->sk;
	int rc = 0;
	if (addr_len < sizeof(addr->sa_family)) {
		return -EINVAL;
	}

	if (addr->sa_family == AF_UNSPEC) {
		sk->ops->disconnect(sk, flags);
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

        sk->ops->connect(sk, addr, addr_len, flags); /* ���������net_ops�еķ�����connnect */
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
	return sk->ops->write(sk, buf, len);
}

int
inet_read(struct socket *sock, void *buf, int len)
{
	struct sock *sk = sock->sk;
	return sk->ops->read(sk, buf, len);
}

/* inet_lookup ���ݶ˿ں�Ѱ�Ҷ�Ӧ��socket */
struct sock *
	inet_lookup(int protocol, uint16_t sport, uint16_t dport)
{
	/* udp��tcp��Ҫ���ֶԴ� */
	struct socket * sock = socket_lookup(protocol, sport, dport);
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

/*
int
inet_abort(struct socket *sock)
{
	struct sock *sk = sock->sk;

	if (sk) {
		sk->ops->abort(sk);
	}
	return 0;
}

*/

