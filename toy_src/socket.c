#include "syshead.h"
#include "utils.h"
#include "socket.h"
#include "inet.h"
#include "wait.h"

static int sock_amount = 0;
static LIST_HEAD(sockets);
static pthread_rwlock_t slock = PTHREAD_RWLOCK_INITIALIZER;

extern struct net_family inet;

/* AF_INET=2 */
static struct net_family *families[128] = {
	/* ���﴿����Ϊ�˱���unix������һ��Ӱ��,ʵ���Ͽ���ȥ���� */
	[AF_INET] = &inet,
};


#ifdef DEBUG_SOCKET
void 
socket_debug()
{
	struct list_head *item;
	struct socket *sock = NULL;
	pthread_rwlock_rdlock(&slock);
	
	list_for_each(item, &sockets) {
		sock = list_entry(item, struct socket, list);
		socket_dbg(sock);
	}

	pthread_rwlock_unlock(&slock);
}
#else
void
socket_debug()
{
	return;
}
#endif

static struct socket *
alloc_socket(pid_t pid)
{
	/* todo: ������Ҫ���һ�ַ���ʹ�����ǵ�fd�����kernel��fd�ص�,
	 ���������fd���õ÷ǳ���. */
	static int fd = 4097;
	struct socket *sock = malloc(sizeof(struct socket));
	list_init(&sock->list);

	sock->pid = pid;	/* ����id */
	sock->fd = fd++;	/* ���ظ��ļ������� */
	sock->ops = NULL;
	sock->flags = O_RDWR;
	wait_init(&sock->sleep);
	return sock;
}

int
socket_free(struct socket *sock)
{
	if (sock->ops) {
		sock->ops->free(sock);
	}
	pthread_rwlock_wrlock(&slock);
	list_del(&sock->list);
	wait_free(&sock->sleep);
	free(sock);
    sock_amount--;
	pthread_rwlock_unlock(&slock);
	return 0;
}


static struct socket *
get_socket(pid_t pid, int fd)
{
	struct list_head *item;
	struct socket *sock = NULL;

	list_for_each(item, &sockets) {
		sock = list_entry(item, struct socket, list);
		if (sock->pid == pid && sock->fd == fd) return sock;
	}
	return NULL;
}



/* 
 ���µ�һϵ�к��������Ǿ���ʹ�õĺ����ǳ�����,ȷʵ,����ȷʵ������ͼ��ԭһϵ�е�����ϵͳ����.
 ����ĺ���һ�㶼�ǵ���sock->ops->xxx,opsָ���ǲ���socket��һϵ�к���.�����xxx,
 �����ӿں����Ǿ���ʹ�õĻ�������һ�µ�. 
 
 ������Ա�֤,����Ĳ���ȫ������Ч��.���Կ���ɾ����������Ĵ���.
 */

/* _socket��������һ��socket,���ҽ�����뵽connections�������֮�� */
int
_socket(pid_t pid, int domain, int type, int protocol)
{
	struct socket *sock;
	struct net_family *family;
	if ((sock = alloc_socket(pid)) == NULL) {
		print_err("Could not alloc socket\n");
		return -1;
	}
	sock->type = type;
	family = families[domain];

	if (!family) {
		print_err("Domain not supprted: %d\n", domain);
		goto abort_socket;
	}

	if (family->create(sock, protocol) != 0) {	/* ����һ��sock,�����create
										�����ջ��,ʵ�ʵ���inet_create���� */
		print_err("Creating domain failed\n");
		goto abort_socket;
	}

	pthread_rwlock_wrlock(&slock);
	list_add_tail(&sock->list, &sockets); /* ���¹�����socket����connections���� */
    sock_amount++;
	pthread_rwlock_unlock(&slock);
	return sock->fd;					 /* sock->fdֻ��һ����־���� */

abort_socket:
	socket_free(sock);
	return -1;
}

int 
_bind(pid_t pid, int sockfd, struct sockaddr_in *skaddr)
{
	struct socket *sock;

	if ((sock = get_socket(pid, sockfd)) == NULL) {
		print_err("Bind: could not find socket (fd %d) for binding (pid %d)\n",
			sockfd, pid);
		return -1;
	}
	return sock->ops->bind(sock, skaddr);	/* ʵ���ϵ�����inet_bind */
}

int
_connect(pid_t pid, int sockfd, const struct sockaddr_in *addr)
{
	struct socket *sock;

	if ((sock = get_socket(pid, sockfd)) == NULL) {
		print_err("Connect: could not find socket (fd %d) for connection (pid %d)\n",
		sockfd, pid);
		return -1;
	}
	return sock->ops->connect(sock, addr);
}

/* write ��pid�������ӵĵ�sockfd���ļ�д������ */
int 
_write(pid_t pid, int sockfd, const void *buf, const unsigned int count)
{
	struct socket *sock;
	if ((sock = get_socket(pid, sockfd)) == NULL) {
		print_err("Write: could not find socket (fd %d && pid %d)\n",
			sockfd, pid);
		return -1;
	}
	return sock->ops->write(sock, buf, count);
}

int
_read(pid_t pid, int sockfd, void* buf, const unsigned int count)
{
	struct socket *sock;
	if ((sock = get_socket(pid, sockfd)) == NULL) {
		print_err("Read: could not find socket (fd %d && pid %d)\n",
			sockfd, pid);
		return -1;
	}
	return sock->ops->read(sock, buf, count);
}

int
_close(pid_t pid, int sockfd)
{
	struct socket *sock;
	if ((sock = get_socket(pid, sockfd)) == NULL) {
		print_err("Close: could not find socket (fd %d && pid %d)\n",
			sockfd, pid);
		return -1;
	}
	return sock->ops->close(sock);
}


int
_listen(pid_t pid, int sockfd, int backlog)
{
	int err = -1;
	struct socket *sock;
	if (!sock || backlog < 0) goto out;
	if ((sock = get_socket(pid, sockfd)) == NULL) {
		print_err("listen: could not find socket (fd %d) for \
			listening (pid %d)\n",
			sockfd, pid);
		return -1;
	}
	return sock->ops->listen(sock, backlog);
out:
	return err;
}

int
	_accept(pid_t pid, int sockfd, struct sockaddr_in *skaddr)
{
	struct socket * sock, *newsock;
	int rc = -1, err;

	if ((sock = get_socket(pid, sockfd)) == NULL) {
		print_err("Accept: could not find socket (fd %d && pid %d)\n",
			sockfd, pid);
		return -1;
	}
	newsock = alloc_socket(pid);
	newsock->ops = sock->ops;

	err = sock->ops->accept(sock, newsock, skaddr);
	if (err < 0) {
		free(newsock);
		newsock = NULL;
	}
	else {
		pthread_rwlock_wrlock(&slock);
		list_add_tail(&newsock->list, &sockets); 
		sock_amount++;
		pthread_rwlock_unlock(&slock);
		rc = newsock->fd;	/* ���ض�Ӧ���ļ������� */
	}
out:
	return rc;
}