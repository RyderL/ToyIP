#include "syshead.h"
#include "checksum.h"


uint32_t 
sum_every_16bits(void *addr, int count)
{
	register uint32_t sum = 0;
	uint16_t *ptr = addr;
	uint16_t answer = 0;

	while (count > 1) {
		/*  This is the inner loop */
		sum += *ptr++;
		count -= 2;
	}

	if (count == 1) {
		/*
		 ������һ��ϸ����Ҫע��һ��. unsigned char 8bit
		 answer 16bit			��answerǿ��ת��Ϊ8bit,��ʹ�����ʣ�µ�8bit�����뵽x��
		 +-----+-----+			+-----+-----+
		 |  8  |  8  |			|xxxxx|     |
		 +-----+-----+			+-----+-----+
		 */
		*(unsigned char *)(&answer) = *(unsigned char *)ptr;
		sum += answer;
	}

	return sum;
}

/* checksum ���ڼ���У��ֵ */
uint16_t 
checksum(void *addr, int count, int start_sum)
{
	uint32_t sum = start_sum;
	sum += sum_every_16bits(addr, count);

	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return ~sum;
}

int 
tcp_udp_checksum(uint32_t saddr, uint32_t daddr, uint8_t proto,
	uint8_t *data, uint16_t len)
{
	/* we need to ensure that saddr and daadr are in netowrk byte order */
	uint32_t sum = 0;
	struct pseudo_head head;
	memset(&head, 0, sizeof(struct pseudo_head));
	/* ��Ҫ��֤�����daddr�Լ�saddr�������ֽ��� */
	head.daddr = daddr;
	head.saddr = saddr;
	/* ����TCP��˵,proto = 0x06,������UDP��˵proto = 0x17 */
	head.proto = proto; /* sizeof(proto) == 1,  
						����ֻ��1���ֽڵ�����,����Ҫת���ֽ��� */
	head.len = htons(len);
	sum = sum_every_16bits(&head, sizeof(struct pseudo_head));
	return checksum(data, len, sum);
}