#ifndef SOCKET_H
#define SOCKET_H
#include "sock.h"
#include "socket.h"
#include "list.h"
#include "wait.h"
#include <inttypes.h>
#include <bits/types.h>
#include <unistd.h>
#include <sys/types.h>


#ifdef DEBUG_SOCKET
#define socket_dbg(sock)																			\
    do {																							\
        print_debug("Socket fd %d pid %d state %d sk_state %d flags %d poll %d sport %d dport %d "  \
                    "sock-sleep %d sk-sleep %d recv-q %d send-q %d",								\
                    sock->fd, sock->pid, sock->state, sock->sk->state, sock->flags,					\
                    sock->sk->poll_events,															\
                    sock->sk->sport, sock->sk->dport, sock->sleep.sleeping,							\
                    sock->sk->recv_wait.sleeping, sock->sk->receive_queue.qlen,						\
                    sock->sk->write_queue.qlen);													\
    } while (0)
#else
#define socket_dbg(sock)
#endif


struct socket;

struct sock_type {
	struct sock_ops *sock_ops;	/* sock_ops��¼һ�׶�socket�Ĳ��ݷ��� */
	struct net_ops *net_ops;
	int type;
	int protocol;
};

struct sock_ops {
	int (*connect)(struct socket *sock, const struct sockaddr_in *addr);
	int(*write) (struct socket *sock, const void *buf, int len);
	int(*read)(struct socket *sock, void *buf, int len);
	int(*accept)(struct socket *, struct socket *, struct sockaddr_in *);
	int(*listen)(struct socket *, int);
	int(*bind)(struct socket *sock, struct sockaddr_in *);	/* �󶨵�ĳ����ַ */
	int(*close)(struct socket *sock);
	int(*free)(struct socket *sock);
	int(*sendto)(int sockfd, const void *buf, size_t len, int flags,
		const struct sockaddr_in *dest_addr);
	int(*recvfrom)(int sockfd, void *buf, size_t len, int flags,
		struct sockaddr_in *src_addr);
};

struct net_family {
	int(*create)(struct socket *sock, int protocol);
};

/* socket���������ײ�,����¼��ʹ�ø�Э��ջ�Ľ���id,��¼��������ӵ�һЩ����. 
  ��sock����ؼ�¼��ʵ�ʵ�����. sock��socket��һ����. */
struct socket {
	struct list_head list;
	int fd;
	pid_t pid;
	short type;
	int flags;
	struct sock *sk;				/* sock��¼���ǹ������ӵ���Ϣ */
	struct sock_ops *ops;			/* ��¼һ�׶�socket���ݵķ��� */
	struct wait_lock sleep;
};


void * socket_ipc_open(void *args);

int _socket(pid_t pid, int domain, int type, int protocol);
int _listen(pid_t pid, int sockfd, int backlog);
int _connect(pid_t pid, int sockfd, const struct sockaddr_in *addr);
int _write(pid_t pid, int sockfd, const void *buf, const unsigned int count);
int _read(pid_t pid, int sockfd, void* buf, const unsigned int count);
int _bind(pid_t pid, int sockfd, struct sockaddr_in *skaddr);
int _close(pid_t pid, int sockfd);
int _accept(pid_t pid, int sockfd, struct sockaddr_in *skaddr);

int socket_free(struct socket *sock);
void socket_debug();

#endif // !SOCKET_H