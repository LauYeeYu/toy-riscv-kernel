#ifndef TOY_RISCV_KERNEL_KERNEL_MEM_MANAGE_H
#define TOY_RISCV_KERNEL_KERNEL_MEM_MANAGE_H

#include "types.h"

/**
 * initialize the memory management
 */
void init_mem_manage();

/**
 * allocate a block of memory with size 2^power * 4KiB
 * @param power the power of 2
 * @return the address of the allocated memory (NULL for failure)
 */
void *allocate(size_t power);

/**
 * deallocate a block of memory with size 2^power * 4KiB
 * @param addr the address of the memory to be deallocated
 * @param power the power of 2
 */
void deallocate(void *addr, size_t power);

#endif //TOY_RISCV_KERNEL_KERNEL_MEM_MANAGE_H
