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
