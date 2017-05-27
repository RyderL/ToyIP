#include "syshead.h"
#include "utils.h"
#include "basic.h"
#include "netdev.h"
#include "skbuff.h"
#include "ethernet.h"
#include "ip.h"
#include "tuntap.h"
#include "arp.h"

struct netdev *loop;
struct netdev *netdev;
extern int running;

//
// addr��ʾip��ַ, hwadddr��ʾmac��ַ, mtu��ʾ����䵥Ԫ�Ĵ�С
static struct netdev *
netdev_alloc(char *addr, char* hwaddr, uint32_t mtu)
{
	/* hwaddr��ʾӲ����ַ */
	struct netdev *dev = malloc(sizeof(struct netdev));
	dev->addr = ip_parse(addr);		/* ��¼��ip��ַ */

	sscanf(hwaddr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		&dev->hwaddr[0],
		&dev->hwaddr[1],
		&dev->hwaddr[2],
		&dev->hwaddr[3],
		&dev->hwaddr[4],
		&dev->hwaddr[5]);				/* ��¼��mac��ַ */

	dev->addr_len = 6;					/* ��ַ���� */
	dev->mtu = mtu;						/* ����䵥Ԫ */
	return dev;
}


void 
netdev_init()
{
	loop = netdev_alloc("127.0.0.1", "00:00:00:00:00:00", 1500);
	/* �����mac��ַ�������. */
	netdev = netdev_alloc("10.0.1.4", "00:0c:29:6d:50:25", 1500);
}

/* netdev_transmit ���ڶ��ϲ㴫�ݹ��������ݰ�װ��̫��ͷ�� */
int 
netdev_transmit(struct sk_buff *skb, uint8_t *dst_hw, uint16_t ethertype)
{
	struct netdev *dev;
	struct eth_hdr *hdr;
	int ret = 0;

	dev = skb->dev;
	skb_push(skb, ETH_HDR_LEN);
	hdr = (struct eth_hdr *)skb->data;

	/* ����Ӳ����ַ */
	memcpy(hdr->dmac, dst_hw, dev->addr_len);
	memcpy(hdr->smac, dev->hwaddr, dev->addr_len);
	
	eth_dbg("out", hdr);
	hdr->ethertype = htons(ethertype);
	/* �ظ�,ֱ��д���� */
	ret = tun_write((char *)skb->data, skb->len);
}

static int 
netdev_receive(struct sk_buff *skb)
{
	struct eth_hdr *hdr = eth_hdr(skb);  /* �����̫��ͷ����Ϣ,��̫��ͷ������
										 Ŀ��mac��ַ,Դmac��ַ,�Լ�������Ϣ */
	eth_dbg("in", hdr);
	/* ��̫��ͷ����Type(����)�ֶ� 0x86dd��ʾIPv6 0x0800��ʾIPv4
	0x0806��ʾARP */
	switch (hdr->ethertype) {
	case ETH_P_ARP:	/* ARP  0x0806 */
		arp_rcv(skb);
		break;
	case ETH_P_IP:  /* IPv4 0x0800 */
		ip_rcv(skb);
		break;
	case ETH_P_IPV6: /* IPv6 0x86dd -- not supported! */
	default:
		printf("Unsupported ethertype %x\n", hdr->ethertype);
		free_skb(skb);
		break;
	}
	return 0;
}

/* netdev_rx_loop */
void *
netdev_rx_loop()
{
	while (running) {
		struct sk_buff *skb = alloc_skb(BUFLEN);		/* 1600 */
		/* skb�Ƕ����ݵ�һ���򵥷�װ,������������skb->data��,skb���������Ƕ����ݵ�һЩ���� */
		/* tun_readÿһ�λ��ȡһ�����ݱ�,��ʹ�����ݳ��ȴﲻ��1600 */
		int len = tun_read((char *)skb->data, BUFLEN);  
		if (len < 0) {									
			perror("ERR: Read from tun_fd");
			free_skb(skb);
			return NULL;
		}
		netdev_receive(skb);
	}
	return NULL;
}

struct netdev* 
netdev_get(uint32_t sip)
{
	if (netdev->addr == sip) {
		return netdev; /* ��static local variable�ĵ�ַ���ݳ�ȥ, netdev����mac��ַ��Ϣ */
	}
	else
	{
		return NULL;
	}
}

void 
free_netdev()
{
	free(loop);
	free(netdev);
}

/**\
 * local_ipaddress�����ж�addr�Ƿ�Ϊ������ַ.
\**/
int
local_ipaddress(uint32_t addr)
{
	/* �����addr�Ǳ����ֽ����ʾ��ip��ַ */
	struct netdev *dev;
	if (!addr) /* INADDR_ANY */
		return 1;
	/* netdev��addr���¼���Ǳ����ֽ����ip��ַ */
	if (addr == netdev->addr) return 1;
	if (addr == loop->addr) return 1;
	return 0;
}

