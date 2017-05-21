#include "syshead.h"
#include "skbuff.h"
#include "utils.h"
#include "ip.h"
#include "sock.h"
#include "route.h"


int
ip_output(struct sock *sk, struct sk_buff *skb)
{
	struct rtentry *rt;
	struct iphdr *ihdr = ip_hdr(skb);

	rt = route_lookup(ihdr->daddr);	/* ����Ŀ��ip��ַ����·�� */

	if (!rt) {
		/* todo */
		return -1;
	}

	skb->dev = rt->dev;				/* dev����ָʾ */
	skb->rt = rt;
	skb_push(skb, IP_HDR_LEN);		/* ipͷ�� */

	ihdr->version = IPV4;			/* ip�İ汾��IPv4 */
	ihdr->ihl = 0x05;				/* ipͷ��20�ֽ�,Ҳ����˵�������κ�ѡ�� */
	ihdr->tos = 0;					/* tosѡ��������TCP/IPʵ����֧��  */
	ihdr->len = skb->len;			/* ����ip���ݱ��Ĵ�С */
	ihdr->id = ihdr->id;			/* id���� */
	ihdr->flags = 0;
	ihdr->frag_offset = 0;
	ihdr->ttl = 64;
	ihdr->proto = skb->protocol;
	ihdr->saddr = skb->dev->addr;
	ihdr->daddr = sk->daddr;
	ihdr->csum = 0;

	ip_dbg("out", ihdr);

	ihdr->len = htons(ihdr->len);
	ihdr->id = htons(ihdr->id);
	ihdr->daddr = htonl(ihdr->daddr);
	ihdr->saddr = htonl(ihdr->saddr);
	ihdr->csum = htons(ihdr->csum);
	ihdr->csum = checksum(ihdr, ihdr->ihl * 4, 0);


	return dst_neigh_output(skb);
}