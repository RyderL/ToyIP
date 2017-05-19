#ifndef TIMER_H
#define TIMER_H

#include "syshead.h"
#include "utils.h"
#include "list.h"

#define timer_dbg(msg, t)													\
	do {																	\
		print_debug("Timer at %d: "msg": expires %d", tick, t->expires);	\
	} while(0)

struct timer {
	struct list_head list;
	int refcnt;					// ���ü���
	uint32_t	expires;		// ����ʱ��
	int cancelled;				// �Ƿ���Ҫȡ��
	void (*handler)(uint32_t, void *); // ������
	void *arg;						   // ���ò���
};

struct timer *timer_add(uint32_t expire, void(*handler)(uint32_t, void*), void *arg);
void timer_release(struct timer *t);
void timer_cancel(struct timer *t);
void *timers_start();
int timer_get_tick();


#endif // !TIMER_H