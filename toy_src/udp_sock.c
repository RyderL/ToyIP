#include "syshead.h"
#include "utils.h"
#include "ip.h"
#include "udp.h"
#include <sys/types.h>

static int udp_sock_amount = 0;
static LIST_HEAD(udp_socks);
static pthread_rwlock_t slock = PTHREAD_RWLOCK_INITIALIZER;

void
udp_socks_enqueue(struct sock *sk)
{
	pthread_rwlock_wrlock(&slock);
	list_add_tail(&sk->link, &udp_socks);
	udp_sock_amount++;
	pthread_rwlock_unlock(&slock);
}

static void
udp_socks_remove(struct sock *sk)
{
	pthread_rwlock_wrlock(&slock);
	udp_sock_amount--;
	list_del(&sk->link);
	pthread_rwlock_unlock(&slock);
}

struct sock *
	udp_lookup_sock(uint16_t dport)
{
	struct sock *sk;
	struct list_head *item;

	pthread_rwlock_rdlock(&slock);
	list_for_each(item, &udp_socks) {
		sk = list_entry(item, struct sock, link);
		if ((sk->dport == dport) || (sk->dport == 0)) {
			pthread_rwlock_unlock(&slock);
			return sk;
		}
	}
	pthread_rwlock_unlock(&slock);
	return NULL;
}


int udp_recvfrom(struct sock *sk, void *buf, int len, struct sockaddr_in *saddr);
static int udp_set_sport(struct sock *sk, uint16_t sport);
static int udp_recv_notify(struct sock *sk);

struct net_ops udp_ops = {
	.alloc_sock = &udp_alloc_sock,
	.init = &udp_sock_init,
	.send_buf = &udp_write,
	.connect = &udp_connect,
	.sendto = &udp_sendto,
	.recvfrom = &udp_recvfrom,
	.recv_buf = &udp_read,
	.close = &udp_close,
	.set_sport = &udp_set_sport,
	.recv_notify = &udp_recv_notify,
};

/**\
 * udp_init ����udpЭ�������ʲô��Ҫ��ʼ���Ķ���,���Էŵ����������.
\**/
void 
udp_init()
{
	

}

static inline int
udp_recv_notify(struct sock *sk)
{
	if (&(sk->recv_wait)) {
		return wait_wakeup(&sk->recv_wait); /* ���ѵȴ��Ľ��� */
	}
	return -1;
}


int
udp_close(struct sock *sk)
{
	return 0;
}

int
udp_sock_init(struct sock *sk)
{

	return 0;
}


/**\
 *	udp_recvfrom ���ڲ���saddr��ַ���ݹ���������.
\**/
int
udp_recvfrom(struct sock *sk, void *buf, int len, struct sockaddr_in *saddr)
{
	int rc = -1;
	if (saddr) {
		sk->dport = ntohs(saddr->sin_port);
		sk->daddr = ntohl(saddr->sin_addr.s_addr);
	}
	else {
		/* ���Ϊ�յĻ�,��Ҫ���������� */
		sk->dport = 0;
	}
	/* ��sock�ҵ�����ȥ. */
	udp_socks_enqueue(sk);
	rc = udp_read(sk, buf, len);
	/* ����֮��ǵý�sockȡ�� */
	udp_socks_remove(sk);
	return rc;
}

int
udp_connect(struct sock *sk, const struct sockaddr_in *addr)
{
	/* udpû���������ֵĹ���,������ֻ��Ҫ��һЩ���,
	 ���û�д���,�ͼ�¼�Զ˵�IP��ַ�Ͷ˿ں�,��������. */
	extern char * stackaddr;

	// todo: ��ip��ַ�����
	uint16_t dport = addr->sin_port;
	uint32_t daddr = addr->sin_addr.s_addr;
	sk->dport = ntohs(dport);
	sk->daddr = ntohl(daddr);
	sk->saddr = parse_ipv4_string(stackaddr);
	sk->sport = udp_generate_port();	/* �������һ���˿� */
	return 0;
}

int
udp_write(struct sock *sk, const void *buf, int len)
{
	struct udp_sock *usk = udp_sk(sk);

	if (len < 0 || len > UDP_MAX_BUFSZ)
		return -1;
	/* ���Ա�֤,����udp_sendʱ�����ݳ�����������Χ��.���Է��ͳ���Ϊ0��udp���ݱ�. */
	return udp_send(&usk->sk, buf, len);
}

struct sk_buff *
udp_alloc_skb(int size)
{
	int reserved = ETH_HDR_LEN + IP_HDR_LEN + UDP_HDR_LEN + size;
	struct sk_buff *skb = alloc_skb(reserved);
	
	skb_reserve(skb, reserved);
	skb->protocol = IP_UDP; 	/* udpЭ�� */
	skb->dlen = size;
	return skb;
}


int 
udp_sendto(struct sock *sk, const void *buf, int size, const struct sockaddr_in *skaddr)
{
	extern char *stackaddr;
	int rc = -1;
	sk->daddr = ntohl(skaddr->sin_addr.s_addr);
	sk->dport = ntohs(skaddr->sin_port);
	sk->sport = udp_generate_port();
	sk->saddr = ip_parse(stackaddr);
	//udp_socks_enqueue(sk);
	rc = udp_send(sk, buf, size);
	//struct sock *fake_sk = udp_alloc_sock();
	//fake_sk->daddr = ntohl(skaddr->sin_addr.s_addr);
	//fake_sk->dport = ntohs(skaddr->sin_port);
	//fake_sk->sport = udp_generate_port();
	//fake_sk->saddr = ip_parse(stackaddr);
	//udp_free_sock(fake_sk);
	return rc;
}

int
udp_send(struct sock *sk, const void *buf, int len)
{
	struct sk_buff *skb;
	struct udphdr *udphd;

	// tofix: ������Ҫ�����ݷ�Ƭ,������ݹ���Ļ�,��Ȼ,��Ӧ�÷�����ip�� 
	skb = udp_alloc_skb(len);
	skb_push(skb, len);
	memcpy(skb->data, buf, len);

	skb_push(skb, UDP_HDR_LEN);
	udphd = udp_hdr(skb);

	udphd->sport = sk->sport;
	udphd->dport = sk->dport;
	udphd->len = skb->len;

	udpdbg("udpout");

	udphd->sport = htons(udphd->sport);
	udphd->dport = htons(udphd->dport);
	udphd->len = htons(udphd->len);
	udphd->csum = udp_checksum(skb, htonl(sk->saddr), htonl(sk->daddr));
	return ip_output(sk, skb);
}

struct sock *
	udp_alloc_sock()
{
	struct udp_sock *usk = malloc(sizeof(struct udp_sock));
	memset(usk, 0, sizeof(struct udp_sock));
	usk->sk.ops = &udp_ops;
	return &usk->sk;
}

void 
udp_free_sock(struct sock *sk)
{
	struct udp_sock *usk = udp_sk(sk);
	free(usk);
}


int
udp_read(struct sock *sk, void *buf, int len)
{
	/* udp���Զ�0���ֽ�. */
	struct udp_sock *usk = udp_sk(sk);
	int rlen = 0;
	if (len < 0) return -1;

	memset(buf, 0, len);

	for (;;) {
		rlen = udp_data_dequeue(usk, buf, len);
		/* rlen != -1��ʾ�Ѿ�������һ��udp���ݱ�,���Է�����. */
		if (rlen != -1) break;

		/* ������rlen == -1,��ʾ��ʱû��udp���ݿɶ�ȡ */
		wait_sleep(&sk->recv_wait);
	}
	return rlen;
}


static int
udp_port_used(uint16_t pt)
{
	struct sock *sk;
	struct list_head* item;
	list_for_each(item, &udp_socks) {
		sk = list_entry(item, struct sock, link);
		if (sk->sport == pt) {
			return 1;
		}
	}
	return 0;
}

static int
udp_set_sport(struct sock *sk, uint16_t sport)
{
	int rc = -1;
	if (!sport || udp_port_used(sport)) {
		goto out;
	}
	sk->sport = sport;
out:
	return rc;
}

