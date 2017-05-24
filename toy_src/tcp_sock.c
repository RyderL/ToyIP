#include "syshead.h"
#include "tcp.h"

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/* Ϊ�˾����ܼ��,����ʵ�ַǳ���. */


LIST_HEAD(tcp_connecting_or_listening_socks);
LIST_HEAD(tcp_establised_or_syn_recvd_socks);


// tofix: ����

void inline
tcp_established_or_syn_recvd_socks_enqueue(struct sock *sk)
{
	list_add_tail(&sk->link, &tcp_establised_or_syn_recvd_socks);
}


void inline
tcp_established_or_syn_recvd_socks_remove(struct sock *sk)
{
	list_del(&sk->link);
}

void inline
tcp_connecting_or_listening_socks_enqueue(struct sock *sk)
{
	list_add_tail(&sk->link, &tcp_connecting_or_listening_socks);
}

void inline
tcp_connecting_or_listening_socks_remove(struct sock *sk)
{
	list_del(&sk->link);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
static int tcp_set_sport(struct sock *sk, uint16_t port);
static int tcp_listen(struct sock *sk, int backlog);
static struct sock* tcp_accept(struct sock* sk);

struct net_ops tcp_ops = {
	.alloc_sock = &tcp_alloc_sock,
	.init = &tcp_sock_init,
	.connect = &tcp_v4_connect,
	.accept = &tcp_accept,
	.listen = &tcp_listen,
	.send_buf = &tcp_write,
	.recv_buf = &tcp_read,
	.recv_notify = &tcp_recv_notify,	/* recv_notify���ڻ��ѵ��ø�Э��ջ��Ӧ�ó��� */
	.close = &tcp_close,
	.set_sport = &tcp_set_sport,
};

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
struct sock *
	tcp_alloc_sock()
{
	struct tcp_sock *tsk = malloc(sizeof(struct tcp_sock));
	memset(tsk, 0, sizeof(struct tcp_sock));
	tsk->sk.state = TCP_CLOSE;		/* �ʼ���ڹر�״̬ */
	tsk->flags = 0;
	tsk->backoff = 0;

	tsk->retransmit = NULL;
	tsk->delack = NULL;
	tsk->keepalive = NULL;

	tsk->delacks = 0;
	tsk->parent = NULL;


	tsk->sk.ops = &tcp_ops;
	/* todo: determine mss properly */

	/* ��������Ķγ��� */
	tsk->rmss = 1460;
	tsk->smss = 1460;

	wait_init(&tsk->wait);
	skb_queue_init(&tsk->ofo_queue);
	list_init(&tsk->accept_queue);
	list_init(&tsk->listen_queue);
	return (struct sock *)tsk;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/**\
 * tcp_init ������tcpЭ����һЩ��ʼ������.
\**/
int
tcp_init()
{
	return 0;
}

/**\
 * tcp_sock_init ÿ����һ��tcp_sock,���������������Ը�tcp_sock����ʼ�� 
\**/
int
tcp_sock_init(struct sock *sk)
{
	return 0;
}


/**\
 * tcp_port_used�����ж�port�˿��Ƿ��Ѿ���ʹ��.
\**/
static int
tcp_port_used(uint16_t pt)
{
	struct list_head* item;
	struct sock* it;
	/* ������������,�鿴�Ƿ�port�Ѿ���ʹ�� */
	list_for_each(item, &tcp_connecting_or_listening_socks) {
		it = list_entry(item, struct sock, link);
		if (it->sport == pt) return 1;
	}
	return 0;
}

/**\
 * tcp_set_sport��������Դ�˿�,��Ҫ��֤,port�������ֽ���.
\**/
static int
tcp_set_sport(struct sock *sk, uint16_t port)
{
	int err = -1;
	/* �˿ںŲ���Ϊ0 */
	if (!port) goto out;
	/* �˿��Ѿ���ռ�� */
	if (port && tcp_port_used(port)) goto out;

	sk->sport = port;
	err = 0;
out:
	return err;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int
tcp_write(struct sock *sk, const void *buf, int len)
{
	struct tcp_sock *tsk = tcp_sk(sk);
	int ret = -1;

	switch (sk->state) {
		/* ֻ��established��close_wait����״̬������Զ˷������� */
	case TCP_ESTABLISHED:
	case TCP_CLOSE_WAIT: /* close_wait״ָ̬���ǽ��յ��˶Զ˷��͵�fin,
						 ���Ǵ˶˻�������Զ˷������� */
		break;
	default:
		goto out;
	}
	return tcp_send(tsk, buf, len);
out:
	return ret;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int
tcp_read(struct sock *sk, void *buf, int len)
{
	struct tcp_sock *tsk = tcp_sk(sk);
	int ret = -1;

	switch (sk->state) {
	case TCP_CLOSE:
		print_err("error: connectin does not exists\n");
		goto out;
	case TCP_LISTEN:
	case TCP_SYN_SENT:
	case TCP_SYN_RECEIVED:
	case TCP_ESTABLISHED:
	case TCP_FIN_WAIT_1:
	case TCP_FIN_WAIT_2:
		break;
	case TCP_CLOSE_WAIT: /* ������close_wait״̬֮��,�Է���Ӧ���ٷ���tcp���ݹ��� */
		if (!skb_queue_empty(&tsk->sk.receive_queue)) break;
		if (tsk->flags & TCP_FIN) {
			tsk->flags &= ~TCP_FIN;
			return 0;
		}
		break;
		/* closing --����ͬʱ�ر�,������fin,���ҽ��յ��˶Է���fin,��ʱ����closing״̬;
		last_ack --�������Է�������fin,ֻ��Ҫ���նԷ���ACK,�Ϳ��Խ���closed״̬;
		time_wait --�ͻ��˽��յ��˴���last_ack״̬�ķ���˷��͵�fin,���һ�������˷���ack;
		���ϵ���Щ״̬�������ܷ���tcp���ݸ��Է�. */
	case TCP_CLOSING:
	case TCP_LAST_ACK:
	case TCP_TIME_WAIT:
		print_err("error:  connection closing\n");
		goto out;
	default:
		goto out;
	}
	return tcp_receive(tsk, buf, len);
out:
	return ret;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int
tcp_close(struct sock *sk)
{
	switch (sk->state) {
		/* ������Щ״̬�Ѿ����ڴ���close,�����ٴ�ִ��close */
	case TCP_CLOSE:
	case TCP_CLOSING:
	case TCP_LAST_ACK:
	case TCP_TIME_WAIT:
	case TCP_FIN_WAIT_1:
	case TCP_FIN_WAIT_2:
		sk->err = -EBADF;
		return -1;
		/* �����һ�㶼����listen״̬ */
	case TCP_LISTEN:
		/* �ͻ��������˷�����syn,����syn_sent״̬ */
		tcp_set_state(sk, TCP_CLOSE);
		/* �����tcp_connecting_or_listening_socks��ɾ�� */
		tcp_connecting_or_listening_socks_remove(sk);
		break;
	case TCP_SYN_SENT:
		/* ����˽��ܵ������ڴ���syn_sent״̬�Ŀͻ��˷��͵�syn,����syn_received״̬,
		���������,Ҫ��Է�����syn��ack */
	case TCP_SYN_RECEIVED:
	case TCP_ESTABLISHED:
		tcp_queue_fin(sk);	/* ��Զ˷���fin,����fin_wait_1״̬ */
		tcp_set_state(sk, TCP_FIN_WAIT_1);
		break;
	case TCP_CLOSE_WAIT:
		/* close_wait���ڱ����ر�,�������״̬��������Զ˷�������,����һ����˷�������fin
		��������lask_ack״̬ */
		tcp_queue_fin(sk);
		tcp_set_state(sk, TCP_LAST_ACK);
		break;
	default:
		print_err("Unknown TCP state for close\n");
		return -1;
	}
	return 0;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int
tcp_recv_notify(struct sock *sk)
{
	if (&(sk->recv_wait)) {
		return wait_wakeup(&sk->recv_wait); /* ���ѵȴ��Ľ��� */
	}
	return -1;
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int
tcp_v4_connect(struct sock *sk, const struct sockaddr_in *addr)
{
	extern char *stackaddr;
	struct tcp_sock *tsk = tcp_sk(sk);
	uint16_t dport = ((struct sockaddr_in *)addr)->sin_port;
	uint32_t daddr = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
	int rc = 0;

	sk->dport = ntohs(dport);
	sk->sport = tcp_generate_port();			  /* α�������һ���˿� */
	sk->daddr = ntohl(daddr);
	sk->saddr = ip_parse(stackaddr);			  /* sk�д洢���������ֽ��� */
	tcp_connecting_or_listening_socks_enqueue(sk);
	tcp_begin_connect(sk);					  /* ������Է�����ack */

	/* ��������Ҫ�ȴ����ӵĳɹ����� */
	wait_sleep(&tsk->wait);
	tcp_connecting_or_listening_socks_remove(sk);
	tcp_established_or_syn_recvd_socks_enqueue(sk);
out:
	return rc;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

static struct sock*
tcp_accept(struct sock* sk)
{
	struct tcp_sock *tsk = tcp_sk(sk);
	struct tcp_sock *newtsk = NULL;	/* �ڴ�����һ���µ����� */

	while (list_empty(&tsk->accept_queue)) {
		if (wait_sleep(&tsk->wait) < 0) goto out;	/* �ȴ������� */
	}

	newtsk = tcp_accept_dequeue(tsk);


	/* ���ӽ����ɹ�, ���ﲻ���ٴμ���,��Ϊ���ѵ�ʱ��newsk�Ѿ��ڶ�������. */
out:
	return newtsk ? &newtsk->sk : NULL;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/**\
 * tcp_lookup_established_or_syn_recvd_sock ������Ѱ��ص�sock.
\**/
static struct sock*
tcp_lookup_establised_or_syn_recvd_sock(uint32_t src, uint16_t sport, uint32_t dst, uint16_t dport)
{
	struct sock *sk;
	struct list_head* item;
	list_for_each(item, &tcp_establised_or_syn_recvd_socks) {
		sk = list_entry(item, struct sock, link);
		if ((sk->saddr == src) && (sk->sport == sport) &&
			(sk->daddr == dst) && (sk->dport == dport)) {
			return sk;
		}
	}
	return NULL;
}


/**\
 * tcp_lookup_connecting_or_listening_sock ������Ѱ���ڼ�����sock.
\**/
static struct sock *
	tcp_lookup_connecting_or_listening_sock(uint32_t src, uint16_t sport)
{
	struct sock *sk;
	struct list_head *item;
	list_for_each(item, &tcp_connecting_or_listening_socks) {
		sk = list_entry(item, struct sock, link);
		if ((sk->saddr == src) && (sk->sport == sport))
			return sk;
	}
	return NULL;
}

/**\
 * tcp_connection_exist �����ж������Ƿ��Ѿ�����,��Ҫ�Ǳ�����ͼ�������.
\**/
static int
tcp_connection_exist(uint32_t dst, uint16_t dport)
{
	struct sock *sk;
	struct list_head *item;
	list_for_each(item, &tcp_establised_or_syn_recvd_socks) {
		sk = list_entry(item, struct sock, link);
		if ((sk->daddr == dst) && (sk->dport == dport))
			return 1;
	}
	return 0;
}

/**\
 * tcp_lookup_sock���ݸ�������Ԫ��Ѱ�Ҷ�Ӧ��sock.
\**/
struct sock *
	tcp_lookup_sock(uint32_t src, uint16_t sport, uint32_t dst, uint16_t dport)
{
	struct sock *sk;
	sk = tcp_lookup_establised_or_syn_recvd_sock(dst, dport, src, sport);
	if (!sk) sk = tcp_lookup_connecting_or_listening_sock(dst, dport);
	return sk;
}

static int
tcp_listen(struct sock *sk, int backlog)
{
	/* ������backlogʧЧ */
	struct tcp_sock *tsk = tcp_sk(sk);
	struct tcb *tcb = &tsk->tcb;

	/* ��������Ҫ����һϵ�еļ�� */
	if (!sk->sport) return -1;		/* û�а󶨺ö˿� */
	if (sk->state != TCP_CLOSE && sk->state != TCP_LISTEN)
		return -1;

	sk->state = TCP_LISTEN;		/* �������״̬ */
	/* ��������Ҫ��sk����������� */
	tcp_connecting_or_listening_socks_enqueue(sk);
	return 0;
}