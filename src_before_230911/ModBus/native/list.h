#ifndef __LIST_H__
#define __LIST_H__

//#define LIST_CHECK

typedef struct list_node {
    struct list_node *next;
} list_node_t;

typedef struct {
    list_node_t *first;
    list_node_t *last;
    unsigned int len;
} list_head_t;

#ifdef LIST_CHECK
static inline void list_check(list_head_t *head)
{
    int len = 0;
    list_node_t *pre = NULL;
    list_node_t *node = head->first;
    while (node) {
        pre = node;
        node = node->next;
        len++;
    }
    if (head->len != len) {
        printf("PANIC: list %p, wrong len: %d, %d\n", head, head->len, len);
        while(1);
    }
    if (head->last != pre) {
        printf("PANIC: list %p, wrong head->last: %p, %p, len: %d, %d\n",
                head, head->last, node, head->len, len);
        while(1);
    }
}
#endif

// pick first item.
static inline list_node_t *list_get(list_head_t *head)
{
    list_node_t *node = NULL;
    if (head->len) {
        node = head->first;
        head->first = node->next;
        if (--head->len == 0)
            head->last = NULL;
    }
#ifdef LIST_CHECK
    list_check(head);
#endif
    return node;
}

// append item at end.
static inline void list_put(list_head_t *head, list_node_t *node)
{
    if (head->len++)
        head->last->next = node;
    else
        head->first = node;
    head->last = node;
    node->next = NULL;
#ifdef LIST_CHECK
    list_check(head);
#endif
}

// pick last item.
static inline list_node_t *list_get_last(list_head_t *head)
{
    list_node_t *pre = NULL;
    list_node_t *node = head->first;
    if (!node)
        return NULL;
    while (node->next) {
        pre = node;
        node = node->next;
    }
    if (pre) {
        pre->next = NULL;
        head->last = pre;
    } else {
        head->first = head->last = NULL;
    }
    head->len--;
#ifdef LIST_CHECK
    list_check(head);
#endif
    return node;
}

// insert item at first.
static inline void list_put_begin(list_head_t *head, list_node_t *node)
{
    node->next = head->first;
    head->first = node;
    if (!head->len++)
        head->last = node;
#ifdef LIST_CHECK
    list_check(head);
#endif
}

// delete item
static inline void list_delete(list_head_t *head, list_node_t *node)
{
    list_node_t *pre_node = NULL;
    list_node_t *_node = head->first;
    int found = 0;
    while (_node != NULL) {
        if (node == _node) {
            found = 1;
            break;
        }
        pre_node = _node;
        _node = _node->next;
    }
    if (found == 1) {
        if (pre_node == NULL) {
            // first node
            head->first = head->first->next;
            head->len--;
            if (head->len == 0) {
                head->last = NULL;
            }
        } else {
            pre_node->next = node->next;
            head->len--;
            if (pre_node->next == NULL) {
                head->last = pre_node;
            }
        }
    }
#ifdef LIST_CHECK
    list_check(head);
#endif
}

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#define list_entry(ptr, type) \
    container_of(ptr, type, node)
#define list_entry_safe(ptr, type) \
    ({ \
      list_node_t *__ptr = (ptr); \
      __ptr ? container_of(__ptr, type, node) : NULL; \
    })

#define list_get_entry(head, type) \
    list_entry_safe(list_get(head), type)

#endif
