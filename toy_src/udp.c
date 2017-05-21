#include "udp.h"
#include "checksum.h"

struct net_ops udp_ops = {
	.alloc_sock = &udp_alloc_sock,
	.init = &udp_init_sock,
	//.send = &udp_write,
	.connect = &udp_connect,
	//.sendto = &udp_sendto,
	//.recvfrom = &udp_recvfrom,
	//.read = &udp_read,
	.close = &udp_close,
};

int 
udp_sendto()
{

}

int
udp_recvfrom()
{

}

/* genarate_udp_port �������udp�ӿ�.tcp��udp�Ľӿ�ϵͳ�Ƕ�����. */
static
uint16_t 
generate_udp_port()
{
	/* todo: ���õķ���������port. */
	static int port = 40000;
	return ++port + (timer_get_tick() % 10000);
}

int
udp_connect(struct sock *sk, const struct sockaddr_in *addr)
{
	/* udpû���������ֵĹ���,������ֻ��Ҫ��һЩ���, 
	���û�д���,�ͼ�¼�Զ˵�IP��ַ�Ͷ˿ں�,��������. */
	extern char * stackaddr;

	/* todo: ��ip��ַ����� */
	
	uint16_t dport = addr->sin_port;
	uint32_t daddr = addr->sin_addr.s_addr;
	sk->dport = ntohs(dport);
	sk->daddr = ntohl(daddr);
	sk->saddr = parse_ipv4_string(stackaddr);
	sk->sport = generate_udp_port();	/* �������һ���˿� */
	return 0;
}

int
udp_close(struct sock *sk)
{
	/* udp��������һ��û��״̬��Э��,������ʲô�رղ��ر�. */
	return 0;
}

int
udp_init_sock(struct sock *sk)
{

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

	/* tofix: ������Ҫ�����ݷ�Ƭ,������ݹ���Ļ�,��Ȼ,��Ӧ�÷�����ip�� */
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

int
udp_checksum(struct sk_buff *skb, uint32_t saddr, uint32_t daddr)
{
	return tcp_udp_checksum(saddr, daddr, IP_UDP, skb->data, skb->len);
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
udp_recv(struct sk_buff *skb, struct iphdr *iphd, struct udphdr *udphd)
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

static void
udp_init_segment(struct udphdr *udphd, struct iphdr *iphd, struct sk_buff *skb)
{
	udphd->sport = htons(udphd->sport);
	udphd->dport = htons(udphd->dport);
	udphd->len = htons(udphd->len);
	udphd->csum = htons(udphd->csum);
	skb->payload = udphd->data;
}

void
udp_in(struct sk_buff *skb)
{
	struct iphdr *iphd = ip_hdr(skb);	/* ipͷ�� */
	struct udphdr *udphd = udp_hdr(skb);
	int udplen = 0;

	if (udplen < UDP_HDR_LEN || udplen < udphd->len) {
		udpdbg("udp length is too small");
		goto drop_pkg;
	}

	udp_recv(skb, iphd, udphd);
drop_pkg:
	free_skb(skb);
}

static int
udp_init_pkg(struct sock *sk, struct sk_buff *skb, void *buf, int size)
{

}


struct sock *
	udp_lookup_sock(uint16_t port)
{

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

	for(;;) {
		rlen = udp_data_dequeue(usk, buf, len);
		/* rlen != -1��ʾ�Ѿ�������һ��udp���ݱ�,���Է�����. */
		if (rlen != -1) break;

		/* ������rlen == -1,��ʾ��ʱû��udp���ݿɶ�ȡ */
		wait_sleep(&usk->sk.recv_wait);
	}
	return rlen;
}

/*
 * udp_data_dequeue ȡ��һ�����ݱ�.
 **/
int
udp_data_dequeue(struct udp_sock *usk, void *user_buf, int userlen)
{
	struct sock *sk = &usk->sk;
	struct udphdr *udphd;
	struct sk_buff *skb;
	int rlen = -1;
	/* udp�ɲ���ʲô��ʽЭ��,����,��һ����Ҫע��,һ��userlen��ʵ�ʵ�udp���ݰ�����ҪС,
	  ��ô��Ĳ��ֻᱻ������. */
	pthread_mutex_lock(&sk->receive_queue.lock);
	if (!skb_queue_empty(&sk->receive_queue))
	{
		skb = skb_peek(&sk->receive_queue);
		udphd = udp_hdr(skb);
		rlen = skb->dlen > userlen? userlen : skb->dlen;
		memcpy(user_buf, skb->payload, rlen);
		/* ��ʹ�����ݱ�������û�ж���,ҲҪ������. */
		skb_dequeue(&sk->receive_queue);
		skb->refcnt--;
		free_skb(skb);
	}
	pthread_mutex_unlock(&sk->receive_queue.lock);
	return rlen;
}

/* udp_data_queue ��udp���ݵ���ʱ,�����ݱ��ŵ����ն���β��. */
int
udp_data_queue(struct udp_sock *usk, struct udphdr *udphd, struct sk_buff *skb)
{
	struct sock *sk = &usk->sk;
	int rc = 0;
	skb_queue_tail(&sk->receive_queue, skb);
	return rc;
}




