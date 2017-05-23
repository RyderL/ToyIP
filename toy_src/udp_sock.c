#include "syshead.h"
#include "utils.h"
#include "ip.h"
#include "udp.h"

static int udp_sock_amount = 0;
static LIST_HEAD(sockets);
static pthread_rwlock_t slock = PTHREAD_RWLOCK_INITIALIZER;


struct sock *
	udp_lookup_sock(uint16_t port)
{

}


struct net_ops udp_ops = {
	.alloc_sock = &udp_alloc_sock,
	.init = &udp_sock_init,
	//.send = &udp_write,
	.connect = &udp_connect,
	//.sendto = &udp_sendto,
	//.recvfrom = &udp_recvfrom,
	//.read = &udp_read,
	.close = &udp_close,
};

void udp_init()
{
	
}


int
udp_close(struct sock *sk)
{
	/* udp��������һ��û��״̬��Э��,������ʲô�رղ��ر�. */
	return 0;
}

int
udp_sock_init(struct sock *sk)
{

	return 0;
}


int 
udp_sendto()
{

}

int
udp_recvfrom()
{

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
	// tofix:
	struct udp_sock *usk; // = udp_sock(sk);

	if (len < 0 || len > UDP_MAX_BUFSZ)
		return -1;
	/* ���Ա�֤,����udp_sendʱ�����ݳ�����������Χ��.���Է��ͳ���Ϊ0��udp���ݱ�. */
	return udp_send(&usk->sk, buf, len);
}

static struct sk_buff *
udp_alloc_skb(int size)
{
	int reserved = ETH_HDR_LEN + IP_HDR_LEN + UDP_HDR_LEN + size;
	struct sk_buff *skb = alloc_skb(reserved);
	skb->protocol = IP_UDP; 	/* udpЭ�� */
	skb->dlen = size;
	return skb;
}

int
udp_send(struct sock *usk, const void *buf, int len)
{
	struct sk_buff *skb;
	struct udphdr *udphd;
	int slen = len;

	// tofix: ������Ҫ�����ݷ�Ƭ,������ݹ���Ļ�,��Ȼ,��Ӧ�÷�����ip�� 
	skb = udp_alloc_skb(len);
	skb_push(skb, len);
	memcpy(skb->data, buf, len);
	udphd = udp_hdr(skb);

	udphd->sport = usk->sport;
	udphd->dport = usk->dport;
	udphd->len = skb->len;

	udpdbg("udpout");

	udphd->sport = htons(udphd->sport);
	udphd->dport = htons(udphd->dport);
	udphd->len = htons(udphd->len);
	udphd->csum = udp_checksum(skb, htonl(usk->saddr), htonl(usk->daddr));
	return ip_output(usk, skb);
}

struct sock *
	udp_alloc_sock()
{
	struct udp_sock *usk = malloc(sizeof(struct udp_sock));
	memset(usk, 0, sizeof(struct udp_sock));
	usk->sk.ops = &udp_ops;
	return &usk->sk;
}

static void
udp_process(struct sk_buff *skb, struct iphdr *iphd, struct udphdr *udphd)
{
	struct sock *sk;
	sk = udp_lookup_sock(udphd->dport);
	if (!sk) {			/* ���û���ҵ���Ӧ��socket */
						// icmp_send();
		goto drop;
	}

	list_add_tail(&skb->list, &sk->receive_queue.head);	/* ������ն��� */
	sk->ops->recv_notify(sk);
	//free_sock(sk);
	return;
drop:
	free_skb(skb);
}

int
udp_read(struct sock *sk, void *buf, int len)
{
	/* udp���Զ�0���ֽ�. */
	// tofix:
	struct udp_sock *usk; //udp_sk(sk);
	if (len < 0)
		return -1;
	return udp_receive(usk, buf, len);
}

int
udp_receive(struct udp_sock *usk, void *buf, int len)
{
	int rlen = 0;
	struct sock *sk = &usk->sk;
	struct socket *sock = sk->sock;
	memset(buf, 0, len);

	for (;;) {
		rlen = udp_data_dequeue(usk, buf, len);
		/* rlen != -1��ʾ�Ѿ�������һ��udp���ݱ�,���Է�����. */
		if (rlen != -1) break;

		/* ������rlen == -1,��ʾ��ʱû��udp���ݿɶ�ȡ */
		wait_sleep(&usk->sk.recv_wait);
	}
	return rlen;
}