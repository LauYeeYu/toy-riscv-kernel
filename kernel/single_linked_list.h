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


static inline struct single_linked_list *create_single_linked_list() {
    struct single_linked_list *list =
        (struct single_linked_list *)kmalloc(sizeof(struct single_linked_list));
    if (list == NULL) return NULL;
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    return list;
}

static inline void single_linked_list_destroy(struct single_linked_list *list) {
    if (list == NULL) return;
    struct single_linked_list_node *node = list->head;
    while (node != NULL) {
        struct single_linked_list_node *next = node->next;
        kfree(node);
        node = next;
    }
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

static inline void pop_head(struct single_linked_list *list) {
    if (list->head == NULL) return;
    if (list->head == list->tail) {
        kfree(list->head);
        list->head = NULL;
        list->tail = NULL;
    } else {
        struct single_linked_list_node *node = list->head;
        list->head = node->next;
        kfree(node);
    }
    --(list->size);
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

#endif // TOY_RISCV_KERNEL_KERNEL_SINGLE_LINKED_LIST_H
