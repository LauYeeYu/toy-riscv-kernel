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

/** Buddy pool */
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

    void *tmp = (void *)(KERNEL_START + buddy_size);
    while (index >= 0) {
        if ((size_t)tmp >= start_addr) { // available
            buddy_pool.space[index].next = tmp;
            buddy_pool.space[index].next->next = NULL;
            index--;
            tmp = (void *)((size_t)tmp - (PAGE_SIZE << index));
        } else {
            buddy_pool.space[index].next = NULL;
            index--;
            tmp = (void *)((size_t)tmp + (PAGE_SIZE << index));
        }
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

/** 
 * Useful tools for kernel memory management, especially for small blocks of
 * data.
 */

struct block_meta {
    size_t power;
    size_t count;
    void *free; // the start address of the free space
    struct block_meta *next;
    struct block_meta *prev;
};

struct block_meta *block_list_head = NULL;
struct block_meta *block_list_tail = NULL;

inline size_t remained_size(struct block_meta *block) {
    return (PAGE_SIZE << block->power) - (size_t)(block->free);
}
 
struct header {
    size_t size;
    struct block_meta *block;
};

inline size_t align(size_t size) {
    return (size + 7) & ~7;
}

inline size_t gross_size(size_t size) {
    return align(size) + sizeof(struct header);
}

inline int tail_not_enough(size_t size) {
    if (block_list_tail == NULL) return 1;
    else return remained_size(block_list_tail) < gross_size(size);
}

// Init the block and return its block_meta
static void *init_block(void *block, size_t power) {
    if (block == NULL) return NULL;
    struct block_meta *block_meta = (struct block_meta *)block;
    block_meta->power = power;
    block_meta->count = 0;
    block_meta->free = (void *)((size_t)block + sizeof(struct block_meta));
    block_meta->next = NULL;
    block_meta->prev = NULL;
    return block_meta;
}

void *kmalloc(size_t size) {
    if (tail_not_enough(size)) {
        // We need to allocate block space
        size_t need_size = sizeof(struct block_meta) + gross_size(size);
        size_t power = 0;
        while ((PAGE_SIZE << power) < need_size) power++;
        struct block_meta *block = init_block(allocate(power), power);
        if (block == NULL) return NULL;
        block->prev = block_list_tail;
        if (block_list_head == NULL) {
            block_list_head = block;
            block_list_tail = block;
        } else {
            block_list_tail->next = block;
            block_list_tail = block;
        }
        return kmalloc(size);
    } else {
        struct block_meta *block = block_list_tail;
        struct header *header = block->free;
        void *ret = (void *)((size_t)header + sizeof(struct header));
        block->free = (void *)((size_t)block->free + gross_size(size));
        block->count++;
        header->size = align(size);
        header->block = block;
        return ret;
    }
}

void kfree(void *addr) {
    if (addr == NULL) return;
    struct header *header = (struct header *)((size_t)addr - sizeof(struct header));
    struct block_meta *block = header->block;
    block->count--;
    if (block->count == 0) {
        // Free the block
        if (block == block_list_head) {
            block_list_head = block->next;
        }
        if (block == block_list_tail) {
            block_list_tail = block->prev;
        }
        if (block->prev) {
            block->prev->next = block->next;
        }
        if (block->next) {
            block->next->prev = block->prev;
        }
        deallocate(block, block->power);
    } else if ((size_t)(block->free) == (size_t)addr + header->size) {
        // When the free space is at the end of the block
        block->free = addr - sizeof(struct header);
    }
}
