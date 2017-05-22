#ifndef IP_H
#define IP_H
#include "syshead.h"
#include "ethernet.h"
#include "skbuff.h"
#include "sock.h"

#define IPV4	 0x04
#define IP_TCP	 0x06
#define IP_UDP	 0x11
#define ICMPV4	 0x01

#define IP_HDR_LEN sizeof(struct iphdr)
// ip_len��ʾip���ݱ��Ĵ�С,�������ײ�
#define ip_len(ip) (ip->len - (ip->ihl * 4))

#ifdef DEBUG_IP
#define ip_dbg(msg, hdr)																	\
	 do {																					\
        print_debug("ip "msg" (ihl: %hhu version: %hhu tos: %hhu "							\
                    "len %hu id: %hu flags: %hhu frag_offset: %hu ttl: %hhu "				\
                    "proto: %hhu csum: %hx "												\
                    "saddr: %hhu.%hhu.%hhu.%hhu daddr: %hhu.%hhu.%hhu.%hhu)",				\
                    hdr->ihl,																\
                    hdr->version, hdr->tos, hdr->len, hdr->id, hdr->flags,					\
                    hdr->frag_offset, hdr->ttl, hdr->proto, hdr->csum,						\
                    hdr->saddr >> 24, hdr->saddr >> 16, hdr->saddr >> 8, hdr->saddr >> 0,	\
                    hdr->daddr >> 24, hdr->daddr >> 16, hdr->daddr >> 8, hdr->daddr >> 0);	\
    } while (0)
#else
#define ip_dbg(msg, hdr)
#endif

// iphdr ��ʾipͷ��
// ihl -- �ײ�����ָ�����ײ�ռ32bit�ֵ���Ŀ�����κ�ѡ��,��������һ��4�����ֶ�,���,�ײ������Ϊ
//        15*4=60�ֽ�.
// tos -- ���ֶβ��������tcp/ipʵ����֧��.
// len -- �ܳ����ֶ�ָ��������IP���ݱ��ĳ���,���ֽ�Ϊ��λ,�����ײ������ֶκ��ܳ����ֶ�,�Ϳ���֪
//        ��IP���ݱ����������ݵ� ��ʼλ�úͳ���,���ڸ��ֶγ�16bit,����IP���ݱ���ɴ�65535�ֽ�.
//  id -- ��ʶ�ֶ�,Ψһ�ı�ʶ�������͵�ÿһ�����ݱ�.
// ttl -- ����ʱ��,�������ݱ������Ծ�����·������.
struct iphdr {
	uint8_t ihl : 4;					// 4λ�ײ�����
	uint8_t version : 4;				// 4λ�汾��
	uint8_t tos;						// 8λ��������
	uint16_t len;						// 16λ�ܳ���
	uint16_t id;						// 16λ��ʶ
	uint16_t flags : 3;					// 3λ��־
	uint16_t frag_offset : 13;			// 13λƫ��
	uint8_t ttl;						// 8λ����ʱ��
	uint8_t proto;						// 8λЭ��
	uint16_t csum;						// 16λ�ײ�У���
	uint32_t saddr;						// Դ��ַ
	uint32_t daddr;						// Ŀ�ĵ�ַ
	uint8_t data[];
} __attribute__((packed));

static inline struct iphdr *
ip_hdr(const struct sk_buff *skb)
{
	// ��̫��֡����̫��ͷ��֮����ľ���ipͷ��
	return (struct iphdr *)(skb->head + ETH_HDR_LEN);
}

/* ip_parse ֱ�ӽ��ַ���ʽ��ip��ַת��Ϊ�����ֽ�����ʽ��ip��ַ. */
static inline uint32_t 
ip_parse(char *addr)
{
	uint32_t dst = 0;
	if (inet_pton(AF_INET, addr, &dst) != 1) {
		perror("ERR: Parsing inet address failed");
		exit(1);
	}
	/* ��Ҫע�����inet_pton���ַ���ʽ��ip��ַת��Ϊ�����ֽ�����ʽ��ip��ַ */
	return ntohl(dst);
}

int ip_rcv(struct sk_buff *skb);
int ip_output(struct sock *sk, struct sk_buff *skb);
int dst_neigh_output(struct sk_buff *skb);

#endif // IP_H