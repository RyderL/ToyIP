#include "tuntap.h"
#include "syshead.h"
#include "basic.h"
#include "utils.h"

static int tun_fd;
static char* dev;

char *tapaddr = "10.0.1.5";		/* tap�豸�ĵ�ַ */
char *taproute = "10.0.1.0/24";
char *stackaddr = "10.0.1.4";   /* ��Э��ջģ���ip��ַ */


/**\
 * set_stack_attribute ������Ϊset_if_up �� set_if_address���������������
\**/
static int
set_stack_attribute(char *dev)
{
	struct ifreq ifr;
	struct sockaddr_in addr;
	int sockfd, err = -1;

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, tapaddr, &addr.sin_addr);

	bzero(&ifr, sizeof(ifr));
	strcpy(ifr.ifr_name, dev);
	bcopy(&addr, &ifr.ifr_addr, sizeof(addr));
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		perror("socket");
		return -1;
	}

	// ifconfig tap0 10.0.1.5 #�趨ip��ַ
	if ((err = ioctl(sockfd, SIOCSIFADDR, (void *)&ifr)) < 0) {
		perror("ioctl SIOSIFADDR");
		goto done;
	}

	/* ��ýӿڵı�־ */
	if ((err = ioctl(sockfd, SIOCGIFFLAGS, (void *)&ifr)) < 0) {
		perror("ioctl SIOCGIFADDR");
		goto done;
	}

	/* ���ýӿڵı�־ */
	ifr.ifr_flags |= IFF_UP;
	// ifup tap0 #�����豸
	if ((err = ioctl(sockfd, SIOCSIFFLAGS, (void *)&ifr)) < 0) {
		perror("ioctl SIOCSIFFLAGS");
		goto done;
	}

	inet_pton(AF_INET, "255.255.255.0", &addr.sin_addr);
	bcopy(&addr, &ifr.ifr_netmask, sizeof(addr));
	// ifconfig tap0 10.0.1.5/24 #�趨��������
	if ((err = ioctl(sockfd, SIOCSIFNETMASK, (void *) &ifr)) < 0) {
		perror("ioctl SIOCSIFNETMASK");
		goto done;
	}


done:
	close(sockfd);
	return err;
}

static int 
set_if_route(char *dev, char *cidr)
{
	// ip route add 10.0.1.0/24 dev tap0
	// #���·�ɣ�����Ŀ�ĵ�ַ��10.0.1.0/24���ε����ݰ������ݸ�tap0�ӿ�
	return run_cmd("ip route add %s dev %s", cidr, dev);
}

static int 
set_if_address(char *dev, char *cidr)
{
	// ip address add dev tap0 10.0.1.5/24 # ���·����Ϣ
	return run_cmd("ip address add dev %s local 10.0.1.5/24", dev, cidr);
}

static int 
set_if_up(char* dev)
{
	// ip link set dev tap0 up  # �����豸
	return run_cmd("ip link set dev %s up", dev);
}

static int 
tun_alloc(char *dev)
{
	struct ifreq ifr;
	int fd, err;
	if ((fd = open("/dev/net/tap", O_RDWR)) < 0) {
		perror("Cannot open TUN/TAP dev\n"
			"Make sure one exists with "
			"'$ mknod /dev/net/tap c 10 200'");
		exit(1);
	}
	CLEAR(ifr);  /* ��� */

	/* Flags: IFF_TUN	- TUN device (no Ethernet headers)
	 *		  IFF_TAP	- TAP device
	 *		  
	 *		  IFF_NO_PI - Do not provide packet information
	 */
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	if (*dev) {
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}

	if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
		perror("ERR: Could not ioctl tun");
		close(fd);
		return err;
	}

	strcpy(dev, ifr.ifr_name);
	return fd;
}

int 
tun_read(char *buf, int len)
{
	return read(tun_fd, buf, len);
}

int 
tun_write(char *buf, int len)
{
	return write(tun_fd, buf, len);
}

void 
tun_init()
{
	dev = calloc(10, 1); /* ����10������Ϊ1���ֽڵĿռ��dev */
	tun_fd = tun_alloc(dev);

	//set_stack_attribute(dev);

	if (set_if_up(dev) != 0) {
		print_err("ERROR when setting up if\n");
	}

	if (set_if_address(dev, tapaddr) != 0) {
		print_err("ERROR when setting addr for if \n");
	}
}

void 
free_tun()
{
	free(dev);
}