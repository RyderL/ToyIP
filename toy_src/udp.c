#include "udp.h"

/**\
 * udp_genarate_port �������udp�ӿ�.tcp��udp�Ľӿ�ϵͳ�Ƕ�����. 
\**/
uint16_t 
udp_generate_port()
{
	// todo: ���õķ���������port
	static int port = 40000;
	return ++port + (timer_get_tick() % 10000);
}


int
udp_checksum(struct sk_buff *skb, uint32_t saddr, uint32_t daddr)
{
	return tcp_udp_checksum(saddr, daddr, IP_UDP, skb->data, skb->len);
}



static void
udp_init_segment(struct udphdr *udphd, struct iphdr *iphd, struct sk_buff *skb)
{
	udphd->sport = htons(udphd->sport);
	udphd->dport = htons(udphd->dport);
	udphd->len = htons(udphd->len);
	udphd->csum = htons(udphd->csum);
	skb->payload = udphd->data;

	skb->dlen = ip_len(iphd) - udphd->len;

}

void
udp_in(struct sk_buff *skb)
{
	struct iphdr *iphd = ip_hdr(skb);	/* ipͷ�� */
	struct udphdr *udphd = udp_hdr(skb);
	struct sock *sk;
	int udplen = 0;

	if (udplen < UDP_HDR_LEN || udplen < udphd->len) {
		udpdbg("udp length is too small");
		goto drop;
	}
	udp_init_segment(udphd, iphd, skb);
	// todo: ���У��ֵ
	sk = udp_lookup_sock(udphd->dport);

	if (!sk) {
		// tofix: ����icmp���ɴ��Ӧ.
		goto drop;
	}
	/* ֱ�ӽ����ݼ��뵽���ն��е�β������. */
	skb_queue_tail(&sk->receive_queue, skb);
	sk->ops->recv_notify(sk);
drop:
	free_skb(skb);
}





/**\
 * udp_data_dequeue ȡ��һ�����ݱ�.
\**/
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

/**\ 
 * udp_data_enqueue ��udp���ݵ���ʱ,�����ݱ��ŵ����ն���β��. 
\**/
int
udp_data_enqueue(struct udp_sock *usk, struct udphdr *udphd, struct sk_buff *skb)
{
	struct sock *sk = &usk->sk;
	skb_queue_tail(&sk->receive_queue, skb);
	return 0;
}




