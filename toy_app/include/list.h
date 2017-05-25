#ifndef _LIST_H
#define _LIST_H
#include <stddef.h>

struct list_head 
{
	struct list_head *next;
	struct list_head *prev;
};

// �½�һ��listͷ��
#define LIST_HEAD(name)		\
    struct list_head name = { &(name), &(name) }

static inline void 
list_init(struct list_head *head)
{
	head->prev = head->next = head;
}

// list_add ��new_node��ӵ�head֮��
static inline void 
list_add(struct list_head *new_node, struct list_head *head)
{
	head->next->prev = new_node;
	new_node->next = head->next;
	new_node->prev = head;
	head->next = new_node;
}

// list_add_tail ��new_node��ӵ�list��β��
static inline void 
list_add_tail(struct list_head *new_node, struct list_head *head)
{
	head->prev->next = new_node;
	new_node->prev = head->prev;
	new_node->next = head;
	head->prev = new_node;
}

// ��list��ɾ��Ԫ��elem
static inline void 
list_del(struct list_head *elem)
{
	struct list_head *prev = elem->prev;
	struct list_head *next = elem->next;

	prev->next = next;
	next->prev = prev;
}

#define list_entry(ptr, type, member)		\
    ((type *) ((char *)(ptr) - offsetof(type, member)))

#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

#define list_for_each(pos, head)			\
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, p, head)    \
    for (pos = (head)->next, p = pos->next; \
         pos != (head);                     \
         pos = p, p = pos->next)

// �ж�list�Ƿ�Ϊ��
static inline int list_empty(struct list_head *head)
{
	return head->next == head;
}

#endif