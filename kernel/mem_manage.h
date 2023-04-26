#ifndef TOY_RISCV_KERNEL_KERNEL_MEM_MANAGE_H
#define TOY_RISCV_KERNEL_KERNEL_MEM_MANAGE_H

#include "types.h"
#include "defs.h"

/**
 * Initialize the memory management
 */
void init_mem_manage();

/**
 * Allocate a block of memory with size 2^power * 4KiB
 * @param power the power of 2
 * @return the address of the allocated memory (NULL for failure)
 */
void *allocate(size_t power);

/**
 * Deallocate a block of memory with size 2^power * 4KiB
 * @param addr the address of the memory to be deallocated
 * @param power the power of 2
 */
void deallocate(void *addr, size_t power);

/**
 * Allocate a block of memory with any size, especially for the requirement
 * of small size memory.
 * @param size the size of the memory to be allocated
 * @return the address of the allocated memory (NULL for failure)
 */
void *kmalloc(size_t size);

/**
 * Free a block of memory with any size that is allocated by kmalloc.
 * @param addr the address of the memory to be freed (must be allocated by
 *             kmalloc)
 */
void kfree(void *addr);

#ifdef PRINT_BUDDY_DETAIL
/**
 * print the buddy pool
 */
void print_buddy_pool();
#endif // PRINT_BUDDY_DETAIL

#ifdef TOY_RISCV_KERNEL_TEST_MEM_MANAGE
void test_mem_manage();
#endif // TOY_RISCV_KERNEL_TEST_MEM_MANAGE

#endif //TOY_RISCV_KERNEL_KERNEL_MEM_MANAGE_H
