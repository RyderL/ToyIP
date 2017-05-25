#include "ip.h"
#include "syshead.h"
#include "arp.h"
#include "tcp.h"
#include "udp.h"
#include "icmpv4.h"
//
// ip_input.c ��Ҫ���ڽ���ip���ݰ�.
// 

// ip_init_pkt���ڽ��յ���ip���ݱ�����һ���̶ȵĽ���,Ҳ���ǽ������ֽ���ת��Ϊ�����ֽ���
// �������Ĳ���.
static void
ip_init_pkt(struct iphdr *ih)
{
	ih->saddr = ntohl(ih->saddr);	/* Դip��ַ */
	ih->daddr = ntohl(ih->daddr);	/* Ŀ��ip��ַ */
	ih->len = ntohs(ih->len);		/* 16λ�ܳ��� */
	ih->id = ntohs(ih->id);			/* Ψһ�ı�ʶ */
}


/**\
 * ip_pkt_for_us �ж����ݰ��Ƿ��Ǵ��ݸ����ǵ�.
\**/
static int
ip_pkt_for_us(struct iphdr *ih)
{
	extern char * stackaddr;
	return ih->daddr == ip_parse(stackaddr) ? 1 : 0;
}

int
ip_rcv(struct sk_buff *skb)
{
	struct iphdr *ih = ip_hdr(skb);
	uint16_t csum = -1;


	if (ih->version != IPV4) { // 0x0800 IPv4
		print_err("Datagram version was not IPv4\n");
		goto drop_pkt;
	}

	if (ih->ihl < 5) {
		// 5 * 32bit = 20�ֽ�
        print_err("IPv4 header length must be at least 5\n");
		goto drop_pkt;
	}

	if (ih->ttl == 0) {
		print_err("Time to live of datagram reached 0\n");
		goto drop_pkt;
	}

	csum = checksum(ih, ih->ihl * 4, 0);

	if (csum != 0) {
		/* ��Ч�����ݱ� */
		goto drop_pkt;
	}

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	  todo: ip���ݱ�����
	  ipЭ�鲢����һ���ɿ���Э��,��û��tcpЭ����ش���ȷ�ϻ���.
	  ���ϲ�Ӧ�����ݱ�����,������MTU,��ô��ip���Ҫ���в��,��
	  �����ݲ�ֳ�С���ݷ��ͳ�ȥ,�Է����յ�֮��ҲҪ�������.
	 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	ip_init_pkt(ih);
	ip_dbg("in", ih);

	if (!ip_pkt_for_us(ih)) goto drop_pkt;
    switch (ih->proto) {
    case ICMPV4:
        icmpv4_incoming(skb);
        return 0;
    case IP_TCP:
        tcp_in(skb);
        return 0;
	case IP_UDP:
		udp_in(skb);
		return 0;
    default:
        print_err("Unknown IP header proto\n");
        goto drop_pkt;
    }

drop_pkt:
	free_skb(skb);
	return 0;
}