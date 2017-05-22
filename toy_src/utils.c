#include "syshead.h"
#include "utils.h"
#include <assert.h>

//
// ����ļ���Ҫ����Debug.
// 
extern int debug;

// run_cmd ����ִ��ĳ������
int 
run_cmd(char *cmd, ...)
{
	va_list ap;
	char buf[CMDBUFLEN];
	va_start(ap, cmd);
	vsnprintf(buf, CMDBUFLEN, cmd, ap);
	va_end(ap);
	if (debug) { // DEBUGģʽ�������Ϣ
		printf("EXEC: %s\n", buf);
	}
	return system(buf);
}

void 
print_err(char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}


uint32_t
parse_ipv4_string(char* addr) {
	uint8_t addr_bytes[4];
	sscanf(addr, "%hhu.%hhu.%hhu.%hhu", &addr_bytes[3], &addr_bytes[2], &addr_bytes[1], &addr_bytes[0]);
	return addr_bytes[0] | addr_bytes[1] << 8 | addr_bytes[2] << 16 | addr_bytes[3] << 24;
}

uint32_t
ip_pton(char *addr)
{
	uint32_t dst = 0;
	if (inet_pton(AF_INET, addr, &dst) != 1) {
		perror("ERR: Parsing inet address failed");
		exit(1);
	}
	/* ��Ҫע�����inet_pton���ַ���ʽ��ip��ַת��Ϊ�����ֽ�����ʽ��ip��ַ */
	return dst;
}



/* start checksum */


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

/* end checksum */
