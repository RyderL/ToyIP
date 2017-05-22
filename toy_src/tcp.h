#ifndef TCP_H
#define TCP_H

#include "syshead.h"
#include "ip.h"
#include "timer.h"
#include "utils.h"
#include "sock.h"

#define TCP_HDR_LEN	sizeof(struct tcphdr)
#define TCP_DOFFSET sizeof(struct tcphdr) / 4

#define TCP_FIN	0x01
#define TCP_SYN	0x02
#define TCP_RST	0x04
#define TCP_PSH	0x08
#define TCP_ACK	0x10

#define TCP_URG	0x20
#define TCP_ECN	0x40
#define TCP_WIN 0x80

#define TCP_SYN_BACKOFF 500
#define TCP_CONN_RETRIES 3

#define TCP_OPTLEN_MSS 4
#define TCP_OPT_MSS	  2

#define tcp_sk(sk) ((struct tcp_sock *)sk)

/* tcp�ײ��Ĵ�С,tcpͷ����4��bit����ʾ�ײ�����,�ײ����ȸ������ײ���32bit�ֵ���Ŀ */
#define tcp_hlen(tcp) (tcp->hl << 2)

#ifdef DEBUG_TCP
extern const char *tcp_dbg_states[];
#define tcp_in_dbg(hdr, sk, skb)															  \
do {																						  \
		print_debug("TCP %hhu.%hhu.%hhu.%hhu.%u > %hhu.%hhu.%hhu.%hhu.%u: "					  \
			"Flags [S%hhuA%hhuP%hhuF%hhuR%hhu], seq %u:%u, ack %u, win %u",					  \
			sk->daddr >> 24, sk->daddr >> 16, sk->daddr >> 8, sk->daddr >> 0, sk->dport,	  \
			sk->saddr >> 24, sk->saddr >> 16, sk->saddr >> 8, sk->saddr >> 0, sk->sport,	  \
			hdr->syn, hdr->ack, hdr->psh, hdr->fin, hdr->rst, hdr->seq - tcp_sk(sk)->tcb.irs, \
			hdr->seq + skb->dlen - tcp_sk(sk)->tcb.irs,										  \
			hdr->ack_seq - tcp_sk(sk)->tcb.isn, hdr->win);									  \
	} while (0)

#define tcp_out_dbg(hdr, sk, skb)																	  \
    do {																							  \
        print_debug("TCP %hhu.%hhu.%hhu.%hhu.%u > %hhu.%hhu.%hhu.%hhu.%u: "							  \
                    "Flags [S%hhuA%hhuP%hhuF%hhuR%hhu], seq %u:%u, ack %u, win %u",					  \
                    sk->saddr >> 24, sk->saddr >> 16, sk->saddr >> 8, sk->saddr >> 0, sk->sport,	  \
                    sk->daddr >> 24, sk->daddr >> 16, sk->daddr >> 8, sk->daddr >> 0, sk->dport,	  \
                    hdr->syn, hdr->ack, hdr->psh, hdr->fin, hdr->rst, hdr->seq - tcp_sk(sk)->tcb.isn, \
                    hdr->seq + skb->dlen - tcp_sk(sk)->tcb.isn,										  \
                    hdr->ack_seq - tcp_sk(sk)->tcb.irs, hdr->win);									  \
    } while (0)

#define tcp_sock_dbg(msg, sk)																		 \
    do {																						     \
        print_debug("TCP x:%u > %hhu.%hhu.%hhu.%hhu.%u (snd_una %u, snd_nxt %u, snd_wnd %u, "		 \
                    "snd_wl1 %u, snd_wl2 %u, rcv_nxt %u, rcv_wnd %u) state %s: "msg,				 \
                    sk->sport, sk->daddr >> 24, sk->daddr >> 16, sk->daddr >> 8, sk->daddr >> 0,	 \
                    sk->dport, tcp_sk(sk)->tcb.snd_una - tcp_sk(sk)->tcb.isn,						 \
                    tcp_sk(sk)->tcb.snd_nxt - tcp_sk(sk)->tcb.isn, tcp_sk(sk)->tcb.snd_wnd,			 \
                    tcp_sk(sk)->tcb.snd_wl1, tcp_sk(sk)->tcb.snd_wl2,								 \
                    tcp_sk(sk)->tcb.rcv_nxt - tcp_sk(sk)->tcb.irs, tcp_sk(sk)->tcb.rcv_wnd,			 \
                    tcp_dbg_states[sk->state]);														 \
    } while (0)

#define tcp_set_state(sk, state)					\
    do {											\
        tcp_sock_dbg("state is now "#state, sk);	\
        _tcp_set_state(sk, state);					\
    } while (0)

#else
#define tcp_in_dbg(hdr, sk, skb)
#define tcp_out_dbg(hdr, sk, skb)
#define tcp_sock_dbg(msg, sk)
#define tcp_set_state(sk, state)  __tcp_set_state(sk, state)
#endif

struct tcphdr {
	uint16_t sport;		/* 16λԴ�˿ں� */
	uint16_t dport;		/* 16λĿ�Ķ˿ں� */
	uint32_t seq;		/* 32λ���к� */
	uint32_t ack_seq;	/* 32λȷ�����к�,һ���ʾ��һ�������յ������ݵ����к� */
	uint8_t rsvd : 4;	
	uint8_t hl : 4;		/* 4λ�ײ����� */
	uint8_t fin : 1,	/* ���Ͷ���ɷ������� */
		syn : 1,		/* ͬ�������������һ������ */
		rst : 1,		/* �ؽ����� */
		psh : 1,		/* ���շ�Ӧ�þ��콫������Ķν���Ӧ�ò� */
		ack : 1,		/* ȷ�������Ч */
		urg : 1,		/* ����ָ����Ч */
		ece : 1,
		cwr : 1;
	uint16_t win;		/* 16λ���ڴ�С */
	uint16_t csum;		/* 16λУ��� */
	uint16_t urp;		/* 16λ����ָ�� */
	uint8_t data[];
} __attribute__((packed));


struct tcp_options {
	uint16_t options;
	uint16_t mss;
};

struct tcp_opt_mss {
	uint8_t kind;
	uint8_t len;
	uint16_t mss;
} __attribute__((packed));

struct tcpiphdr {
    uint32_t saddr;
    uint32_t daddr;
    uint8_t zero;
    uint8_t proto;
    uint16_t tlen;
} __attribute__((packed));

enum tcp_states {
	TCP_LISTEN,			/* �ȴ�һ������ */
	TCP_SYN_SENT,		/* �Ѿ�������һ����������,�ȴ��Է��Ļظ� */
	TCP_SYN_RECEIVED,   /* ���յ��˶Է���������syn, ack,��Ҫ����ȷ�� */
	TCP_ESTABLISHED,    /* ���ӽ����ɹ� */
	TCP_FIN_WAIT_1,
	TCP_FIN_WAIT_2,
	TCP_CLOSE,
	TCP_CLOSE_WAIT,
	TCP_CLOSING,
	TCP_LAST_ACK,
	TCP_TIME_WAIT,
};

/* Transmission Control Block ������ƿ� */
struct tcb {
	/* sending side ���ͷ�,ָ���Ǵ˶� */
	uint32_t snd_una; // send unacknowledge #��δ��ȷ�ϵ����ݵ���ʼ���к�
	uint32_t snd_nxt; // send next #��һ��Ҫ���͵�����bit��Ӧ�����к�,��seq
	uint32_t snd_wnd; // send window #���ʹ��ڵĴ�С
	uint32_t snd_up;  // send urgent pointer
	uint32_t snd_wl1; // segment sequence number used for last window update
	uint32_t snd_wl2; // segment acknowledgment number used for last window update
	uint32_t isn;	  // initial send sequence number #��ʼ�����к�(�Լ�������)
	/* receiving side ���շ�,ָ���Ǳ˶� */
	uint32_t rcv_nxt; // receive next #��һ�������յ������ݵ����,һ�����������Է���ack���
	uint32_t rcv_wnd; // receive window #���մ��ڵĴ�С
	uint32_t rcv_up;  // receive urgent pointer
	uint32_t irs;	  // initial receive sequence number #���յ�����ʼ���к�(�Է�����ʼ���к�)
};

/* tcp_sock��ԭ��sock�Ļ����������˺ܶ��µĶ���. */
struct tcp_sock {
	struct sock sk;
	int fd;
	uint16_t tcp_header_len;	/* tcpͷ����С */
	struct tcb tcb;				/* ������ƿ� */
	uint8_t flags;
	uint8_t backoff;
	struct list_head listen_queue;	/* �ȴ����������еĵڶ���ack+syn */
	struct list_head accept_queue;	/* �ȴ����������е����һ�ε�ack */
	struct list_head list;
	struct wait_lock wait;	/* �ȴ����ջ������� */
	//struct wait_lock *wait_connect;	/* �ȴ������� */
	struct tcp_sock *parent;
	struct timer *retransmit;
	struct timer *delack;
	struct timer *keepalive;	/* ���� */
	struct timer *linger;
	uint8_t delacks;
	uint16_t rmss;				/* remote maximum segment size */ 
	uint16_t smss;				/* ����Ķγ��� */
	struct sk_buff_head ofo_queue; /* ofo_queue���ڼ�¼��Щ
								   û�а���˳�򵽴��tcp���ݱ� */
};

static inline struct tcphdr *
tcp_hdr(const struct sk_buff *skb)
{
	return (struct tcphdr *)(skb->head + ETH_HDR_LEN + IP_HDR_LEN);
}


/* tcp_accept_dequeue ��acccept������ȡ��һ��sock */
static struct tcp_sock * 
tcp_accept_dequeue(struct tcp_sock *tsk)
{
	struct tcp_sock *newtsk;
	newtsk = list_first_entry(&tsk->accept_queue, struct tcp_sock, list);
	list_del(&newtsk->list);
	list_init(&newtsk->list);
	return newtsk;
}

/* tcp_accept_enqueue ��tsk���뵽acccept������ */
static inline void
tcp_accept_enqueue(struct tcp_sock *tsk)
{
	if (!list_empty(&tsk->list))
		list_del(&tsk->list);
	list_add(&tsk->list, &tsk->parent->accept_queue);
}

/* tcp_sock.c */
int generate_isn();
int tcp_init_sock();
int tcp_init(struct sock *sk);
int tcp_v4_connect(struct sock *sk, const struct sockaddr_in *addr);
int tcp_write(struct sock *sk, const void *buf, int len);
int tcp_read(struct sock *sk, void *buf, int len);
int tcp_recv_notify(struct sock *sk);
int tcp_close(struct sock *sk);
int tcp_free_sock(struct sock *sk);
int tcp_done(struct sock *sk);

void tcp_established_or_syn_recvd_socks_enqueue(struct sock *sk);
void tcp_connecting_or_listening_socks_enqueue(struct sock *sk);
void tcp_established_or_syn_recvd_socks_remove(struct sock *sk);
void tcp_connecting_or_listening_socks_remove(struct sock *sk);

struct sock* tcp_lookup_sock(uint32_t src, uint16_t sport, uint32_t dst, uint16_t dport);

/* tcp.c */
void tcp_clear_timers(struct sock *sk);
void tcp_stop_retransmission_timer(struct tcp_sock *tsk);
void tcp_release_retransmission_timer(struct tcp_sock *tsk);
void tcp_stop_delack_timer(struct tcp_sock *tsk);
void tcp_release_delack_timer(struct tcp_sock *tsk);
void tcp_handle_fin_state(struct sock *sk);
void tcp_enter_time_wait(struct sock *sk);
void _tcp_set_state(struct sock *sk, uint32_t state);
void tcp_in(struct sk_buff *skb);
void tcp_send_delack(uint32_t ts, void *arg);
int tcp_process(struct sock *sk, struct tcphdr *th, struct sk_buff *skb);
void tcp_enter_time_wait(struct sock *sk);
int tcp_udp_checksum(uint32_t saddr, uint32_t daddr, uint8_t proto, uint8_t *data, uint16_t len);
int tcp_v4_checksum(struct sk_buff *skb, uint32_t saddr, uint32_t daddr);
void tcp_select_initial_window(uint32_t *rcv_wnd);

int generate_isn();
uint16_t tcp_generate_port();
struct sock *tcp_alloc_sock();

/*tcp_output.c*/
int tcp_receive(struct tcp_sock *tsk, void *buf, int len);
void tcp_reset_retransmission_timer(struct tcp_sock *tsk);
int tcp_send_challenge_ack(struct sock *sk, struct sk_buff *skb);
int tcp_send_ack(struct sock *sk);
int tcp_send_fin(struct sock *sk);
int tcp_send(struct tcp_sock *tsk, const void *buf, int len);
int tcp_send_synack(struct sock *sk);
int tcp_begin_connect(struct sock *sk);
void tcp_handle_fin_state(struct sock *sk);
int tcp_queue_fin(struct sock *sk);
int tcp_send_reset(struct tcp_sock *tsk);

/*tcp_data.c*/
int tcp_data_queue(struct tcp_sock *tsk, struct tcphdr *th, struct sk_buff *skb);
int tcp_data_dequeue(struct tcp_sock *tsk, void *user_buf, int userlen);

#endif // !TCP_H