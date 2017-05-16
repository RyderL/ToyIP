#include "syshead.h"
#include "netdev.h"
#include "tuntap.h"
#include "route.h"
#include "ipc.h"
#include "timer.h"
#include "tcp.h"
#include "TEST.h"
#include "tcp.h"
#include "arp.h"
#include "checksum.h"

extern struct netdev* netdev;

// ����У���
uint16_t 
in_checksum(const void* buf, int len)
{
	assert(len % 2 == 0);
	const uint16_t* data = (const uint16_t*)buf;
	int sum = 0;
	for (int i = 0; i < len; i += 2)
	{
		sum += *data++;
	}
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	assert(sum <= 0xFFFF);
	return ~sum;
}


// TEST_TCP_CHECKSUM ����tcp������Ƿ���ȷ
void 
TEST_TCP_CHECKSUM()
{
	// ������鷢��ack
	int len = ETH_HDR_LEN + IP_HDR_LEN + TCP_HDR_LEN;
	struct sk_buff *skb = alloc_skb(len); // ������tcpѡ�������
	skb_reserve(skb, len);  // ��skb��dataָ��ָ��β��
	skb->dlen = 0;  // ʵ�ʵ����ݴ�СΪ0

	struct tcphdr *thdr = tcp_hdr(skb); // ָ��tcpͷ��
	thdr->ack = 1;
	thdr->hl = TCP_DOFFSET;
	skb_push(skb, thdr->hl * 4); // ��skb��dataָ�����tcpͷ����С���ֽ�

	// �˿��������
	thdr->sport = htons(1000);
	thdr->dport = htons(80);
	thdr->seq = htonl(12345678);  /* ���к� */
	thdr->ack_seq = htonl(0);    /* ����ack��־��Чʱ�����������Ч */
	thdr->win = htons(1500);
	thdr->urp = htons(0);		/* ����urg��־��Чʱ������ */
	thdr->csum = 0;

	// ��������������
	thdr->csum = tcp_v4_checksum(skb, ip_pton("10.0.1.4"), ip_pton("10.0.1.5"));
	printf("checksum = 0x%llX\n", thdr->csum);
	// ���濪ʼ�ڶ��ּ���
	thdr->csum = 0;
	int tcp_len = skb->len;
	//skb_push(skb, sizeof(struct tcp_fake_head));
	/*
	struct tcp_fake_head* fk_hdr = (struct tcp_fake_head*)skb->data;
	fk_hdr->src = ip_pton("10.0.1.4");
	fk_hdr->dst = ip_pton("10.0.1.5");
	fk_hdr->zero = 0;
	fk_hdr->protocol = htons(IP_TCP);	
	fk_hdr->tcp_len = htons(tcp_len);
	uint16_t csum = in_cksum(skb->data, skb->len);
	*/
	//printf("checksum = 0x%llX\n", csum);
}

// TEST_SEND_ARP ����ARP
void 
TEST_SEND_ARP()
{
	arp_request(parse_ipv4_string("10.0.1.4"), parse_ipv4_string("10.0.1.5"), netdev);
}
