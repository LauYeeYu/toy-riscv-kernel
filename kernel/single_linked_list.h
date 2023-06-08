#ifndef TOY_RISCV_KERNEL_KERNEL_SINGLE_LINKED_LIST_H
#define TOY_RISCV_KERNEL_KERNEL_SINGLE_LINKED_LIST_H

#include "types.h"
#include "mem_manage.h"

struct single_linked_list_node {
    struct single_linked_list_node *next;
    void *data;
};

struct single_linked_list {
    struct single_linked_list_node *head;
    struct single_linked_list_node *tail;
    int64 size;
};

static inline void init_single_linked_list(struct single_linked_list *list) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

static inline struct single_linked_list *create_single_linked_list() {
    struct single_linked_list *list =
        (struct single_linked_list *)kmalloc(sizeof(struct single_linked_list));
    if (list == NULL) return NULL;
    init_single_linked_list(list);
    return list;
}

static inline void clear_single_linked_list(struct single_linked_list *list) {
    if (list == NULL) return;
    struct single_linked_list_node *node = list->head;
    while (node != NULL) {
        struct single_linked_list_node *next = node->next;
        kfree(node);
        node = next;
    }
    init_single_linked_list(list);
}

static inline void single_linked_list_destroy(struct single_linked_list *list) {
    if (list == NULL) return;
    clear_single_linked_list(list);
    kfree(list);
}

static inline void *head(struct single_linked_list *list) {
    return list->head->data;
}

static inline struct single_linked_list_node *
head_node(struct single_linked_list *list) {
    return list->head;
}

static inline void *tail(struct single_linked_list *list) {
    return list->tail->data;
}

static inline struct single_linked_list_node *
tail_node(struct single_linked_list *list) {
    return list->tail;
}

static inline void pop_head_without_free(struct single_linked_list *list) {
    if (list->head == NULL) return;
    if (list->head == list->tail) {
        list->head = NULL;
        list->tail = NULL;
    } else {
        list->head = list->head->next;
    }
    --(list->size);
}

static inline void pop_head(struct single_linked_list *list) {
    struct single_linked_list_node *node = list->head;
    pop_head_without_free(list);
    kfree(node);
}

static inline struct single_linked_list_node *
make_single_linked_list_node(void *data) {
    typedef struct single_linked_list_node node_t;
    node_t *node = (node_t *)kmalloc(sizeof(node_t));
    if (node == NULL) return NULL;
    node->data = data;
    node->next = NULL;
    return node;
}

/**
 * Push a node to the head of the list.
 * @param list
 * @param data
 * @return 0 if success, -1 if failed.
 */
static inline int push_head(struct single_linked_list *list,
                            struct single_linked_list_node *node) {
    node->next = list->head;
    list->head = node;
    if (list->tail == NULL) list->tail = node;
    ++(list->size);
    return 0;
}

/**
 * Push a node to the tail of the list.
 * @param list
 * @param data
 * @return 0 if success, -1 if failed.
 */
static inline int push_tail(struct single_linked_list *list,
                            struct single_linked_list_node *node) {
    if (list->tail == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
    ++(list->size);
    node->next = NULL;
    return 0;
}

static inline int64 single_linked_list_size(struct single_linked_list *list) {
    return list->size;
}

static inline void for_each_node(struct single_linked_list *list,
                                 void (*func)(void *)) {
    struct single_linked_list_node *node = list->head;
    while (node != NULL) {
        func(node->data);
        node = node->next;
    }
}

static inline int removeAt(struct single_linked_list *list, void *addr) {
    struct single_linked_list_node *node = list->head;
    struct single_linked_list_node *prev = NULL;
    if (node == NULL) return -1;
    prev = node;
    node = node->next;
    while (node != NULL) {
        if (node->data == addr) {
            if (prev == NULL) {
                list->head = node->next;
            } else {
                prev->next = node->next;
            }
            if (node == list->tail) {
                list->tail = prev;
            }
            kfree(node);
            --(list->size);
            return 0;
        }
        prev = node;
        node = node->next;
    }
    return -1;
}

#endif // TOY_RISCV_KERNEL_KERNEL_SINGLE_LINKED_LIST_H
