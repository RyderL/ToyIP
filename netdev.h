#ifndef NETDEV_H
#define NETDEV_H

#define BUFLEN			 1600
#define MAX_ADDR_LEN	 32
// mac��ַ���ַ�����ʾֻ��Ҫ18���ֽ� -- aa:1c:10:80:c5:72 ����'\0'��β
#define MAC_ADDR_STR_LEN 18  

#include <inttypes.h>

#define netdev_dbg(fmt, ...)					\
do {											\
	print_debug("NETDEV:"fmt, ##__VAR_ARGS__);	\
} while (0)	

struct eth_hdr;
struct sk_buff;

struct netdev {
	uint32_t addr;			// ip��ַ
	uint8_t addr_len;		
	uint8_t hwaddr[6];		// mac��ַ,6���ֽ�
	uint32_t mtu;			// mtu,����䵥Ԫ,һ��Ĭ��Ϊ1500�ֽ�
};

void netdev_init();
int netdev_transmit(struct sk_buff *skb, uint8_t *dst, uint16_t ethertype);
void *netdev_rx_loop();
void free_netdev();
struct netdev *netdev_get(uint32_t sip);

#endif // !NETDEV_H
