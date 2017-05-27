#ifndef UDP_H
#define UDP_H

#include "ip.h"
#include "ethernet.h"
#include "sock.h"
#include "skbuff.h"
#include "timer.h"
#include "utils.h"


#ifdef DEBUG_UDP
#define udpdbg(x)

#else
#define udpdbg(x)
#endif

/* udpЭ��ʵ���ϲ�û��״̬,��Щֻ��Ϊ�˴�������趨��α״̬ */
enum udp_state {
	UDP_UNCONNECTED,
	UDP_CONNECTED,
	UDP_CLOSED
};

struct udphdr {
	uint16_t sport;		/* Դ�˿�				*/
	uint16_t dport;		/* Ŀ�Ķ˿�			*/
	uint16_t len;		/* ����,�����ײ������� */
	uint16_t csum;		/* �����				*/
	uint8_t data[];
} __attribute__((packed));

struct udp_sock {
	struct sock sk;
};

#define udp_sk(sk) ((struct udp_sock*)sk)

#define UDP_HDR_LEN sizeof(struct udphdr)
#define UDP_DEFAULT_TTL 64
#define UDP_MAX_BUFSZ (0xffff - UDP_HDR_LEN)

static inline struct udphdr *
udp_hdr(const struct sk_buff *skb)
{
	return (struct udphdr *)(skb->head + ETH_HDR_LEN + IP_HDR_LEN);
}

/* ��TCP���,UDPҪ�򵥺ܶ�,��Ϊ��û��״̬ */
void udp_in(struct sk_buff *skb);
void udp_init(void);
struct sock * udp_alloc_sock();
struct sk_buff* udp_alloc_skb(int size);
int udp_sock_init(struct sock *sk);
int udp_write(struct sock *sk, const void *buf, int len);
int udp_read(struct sock *sk, void *buf, int len);
int udp_send(struct sock *sk, const void *buf, int len);
int udp_connect(struct sock *sk, const struct sockaddr_in *addr);
int udp_sendto(struct sock *sk, const void *buf, int size, const struct sockaddr_in *skaddr);
int udp_recvfrom(struct sock *sk, void *buf, int len, struct sockaddr_in *saddr);
int udp_close(struct sock *sk);
int udp_data_dequeue(struct udp_sock *usk, void *user_buf, int userlen, struct sockaddr_in *saddr);
uint16_t udp_generate_port();

struct sock * udp_lookup_sock(uint16_t dport);
int udp_checksum(struct sk_buff *skb, uint32_t saddr, uint32_t daddr);


#endif // !UDP_H