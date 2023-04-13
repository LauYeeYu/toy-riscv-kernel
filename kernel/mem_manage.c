/**
 * @file mem_manage.c
 * @brief Memory management with buddy algorithm
 */

#include "mem_manage.h"

#include "print.h"
#include "types.h"
#include "defs.h"

// In entry.S
size_t get_kernel_end();

#define BUDDY_MAX_ORDER (15)
#define KERNEL_START (0x80000000ull)
#define KERNEL_MEM_SIZE (128ull * 1024 * 1024)
#define PAGE_SIZE (4096ull)

#ifdef PRINT_BUDDY_DETAIL
const char *capacity[BUDDY_MAX_ORDER + 1] = {
    "4KiB",   "8KiB",   "16KiB",  "32KiB",
    "64KiB",  "128KiB", "256KiB", "512KiB",
    "1MiB",   "2MiB",   "4MiB",   "8MiB",
    "16MiB",  "32MiB",  "64MiB",  "128MiB",
};
#endif // PRINT_BUDDY_DETAIL

typedef struct node {
    struct node *next;
} node;

struct buddy_pool {
    node space[BUDDY_MAX_ORDER + 1];
} buddy_pool;

#ifdef PRINT_BUDDY_DETAIL
void print_buddy_pool();
#endif // PRINT_BUDDY_DETAIL

void init_mem_manage() {
    size_t start_addr = get_kernel_end();
    size_t kernel_size = start_addr - KERNEL_START;
    size_t buddy_size = KERNEL_MEM_SIZE / 2;
    int index = BUDDY_MAX_ORDER - 1;
    buddy_pool.space[BUDDY_MAX_ORDER].next = NULL;
    while (buddy_size >= kernel_size && index >= 0) {
        buddy_pool.space[index].next = (node *)(KERNEL_START + buddy_size);
        // Set the next pointer to NULL
        buddy_pool.space[index].next->next = NULL;
        buddy_size >>= 1;
        index--;
    }
    /* This can be optimized by add the rest of the space to the buddy_pool */
    for (int i = index; i >= 0; i--) {
        buddy_pool.space[i].next = NULL;
    }
#ifdef PRINT_BUDDY_DETAIL
    print_string("\n");
    print_buddy_pool();
#endif // PRINT_BUDDY_DETAIL
}

void *allocate(size_t power) {
    void *addr = NULL;
    size_t level = power;

    // Find the smallest available block
    while (level <= BUDDY_MAX_ORDER) {
        if (buddy_pool.space[level].next != NULL) {
            addr = buddy_pool.space[level].next;
            buddy_pool.space[level].next = buddy_pool.space[level].next->next;
            break;
        }
        level++;
    }

    // No available block
    if (addr == NULL || level > BUDDY_MAX_ORDER) {
        return NULL;
    }

    // Split the block
    while (level > power) {
        level--;
        void *rest = (void *)((size_t)addr + (PAGE_SIZE << level));
        deallocate(rest, level);
    }

    return addr;
}

inline void *remove_tag(void *addr, size_t power) {
    return (void *)((size_t)addr & ~(size_t)(PAGE_SIZE << power));
}

void deallocate(void *addr, size_t power) {
    // Do nothing if the address is NULL
    if (addr == NULL) {
        return;
    }
    // Find the place
    if (!buddy_pool.space[power].next) { // null, no block
        buddy_pool.space[power].next = addr;
        *(size_t *)addr = NULL;
        return;
    }
    size_t odd = (size_t)addr & (PAGE_SIZE << power);

    if (odd) { // odd
        /* prev_prev -> prev -> addr -> next
         * if can merge,
         * prev_prev -> next
         */
        node *prev_prev = &buddy_pool.space[power];
        node *prev = prev_prev->next;
        node *next = prev->next; // prev != NULL has already been checked
        if ((size_t)addr < (size_t)prev) { // head -> addr -> prev
            ((node *)addr)->next = prev;
            prev_prev->next = addr;
            return;
        }
        while (next && (size_t)addr > (size_t)next) {
            prev_prev = prev;
            prev = next;
            next = next->next;
        }
        if ((size_t)remove_tag(addr, power) == (size_t)prev) {
            prev_prev->next = next;
            deallocate(prev, power + 1);
        } else {
            ((node *)addr)->next = next;
            prev->next = addr;
        }
    } else { // even
        /* prev -> addr -> next -> next_next
         * if can merge,
         * prev -> next_next
         */
        node *prev = &buddy_pool.space[power];
        node *next = prev->next;
        while (next && (size_t)next < (size_t)addr) {
            prev = next;
            next = next->next;
        }
        if (next && (size_t)addr == (size_t)remove_tag(next, power)) {
            prev->next = next->next;
            deallocate(addr, power + 1);
        } else {
            ((node *)addr)->next = next;
            prev->next = addr;
        }
    }
}

#ifdef PRINT_BUDDY_DETAIL
void print_buddy_pool() {
    print_string("BUDDY POOL:\n");
    for (int i = 0; i <= BUDDY_MAX_ORDER; i++) {
        node *p = buddy_pool.space[i].next;
        print_string(capacity[i]);
        print_string(": ");
        while (p) {
            print_int((size_t)p, 16);
            print_string(" ");
            p = p->next;
        }
        print_string("\n");
    }
}
#endif // PRINT_BUDDY_DETAIL

#ifdef TOY_RISCV_KERNEL_TEST_MEM_MANAGE
void test_mem_manage() {
    const int max_power = 5;
    const int times = 2;
    void *addr[max_power * times];
    for (int i = 0; i < max_power; i++) {
        for (int j = 0; j < times; j++) {
            addr[times * i + j] = allocate(i);
            print_string("Get ");
            print_int((size_t)addr[2 * i + j], 16);
            print_string("-");
            print_int((size_t)addr[2 * i + j] + (PAGE_SIZE << i) - 1, 16);
            print_string("\n");
            print_buddy_pool();
        }
    }

    for (int i = max_power - 1; i >= 0; i--) {
        for (int j = times - 1; j >= 0; j--) {
            deallocate(addr[times * i + j], i);
            print_string("Free ");
            print_int((size_t)addr[2 * i + j], 16);
            print_string("-");
            print_int((size_t)addr[2 * i + j] + (PAGE_SIZE << i) - 1, 16);
            print_string("\n");
            print_buddy_pool();
        }
    }
}
#endif // TOY_RISCV_KERNEL_TEST_MEM_MANAGE
