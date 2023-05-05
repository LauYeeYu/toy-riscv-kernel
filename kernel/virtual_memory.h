/**
 * @file virtual_memory.h
 * @brief Virtual memory management for kernel.
 * @details
 * This file contains the functions for virtual memory management for both the
 * kernel and user processes. Please note that the pagetable must not contain
 * any huge page.
 */

#ifndef TOY_RISCV_KERNEL_KERNEL_VIRTUAL_MEMORY_H
#define TOY_RISCV_KERNEL_KERNEL_VIRTUAL_MEMORY_H

#include "riscv.h"
#include "types.h"

/**
 * Get the page table entry for virtual address va.
 * NOTE: huge pages are NOT supported!
 * @param pagetable the page table
 * @param va the virtual address
 * @param alloc whether to allocate a page table and page if necessary
 * @return the page table entry, or NULL if the mapping doesn't exist.
 */
pte_t *pagetable_entry(pagetable_t pagetable, uint64 va, int alloc);

/**
 * Convert the virtual address to physical address.
 * @param pagetable the page table
 * @param va the virtual address
 * @return the physical address of the virtual address, 0 if not exists.
 */
uint64 physical_address(pagetable_t pagetable, uint64 va);

/**
 * Create a void page table. If there is no memory, a NULL pointer is returned.
 */
pagetable_t create_void_pagetable();

/**
 * Initialize the virtual memory for user processes.
 * @param pagetable the page table
 * @param src the source address
 * @param size the size of the memory
 * @return the actual size of memory used
 */
uint64 init_virtual_memory_for_user(pagetable_t pagetable,
                                    void *src,
                                    size_t size);

/**
 * Free the memory from [start, start + size).
 * @param pagetable the page table
 * @param start the start address
 * @param size the size of the memory
 */
void free_memory(pagetable_t pagetable, uint64 start, size_t size);

/**
 * Map the page on va to pa in page table.
 * @param pagetable the page table
 * @param va the virtual address
 * @param pa the physical address
 * @param permission the permission of the page, see riscv_defs.h for details
 * @return 0 if success, -1 if failed
 */
int map_page(pagetable_t pagetable,
             uint64 va,
             uint64 pa,
             uint64 permission);

/**
 * Unmap the page on va in page table. The va must be mapped before calling
 * this function.
 * Please note that this function does not free the page.
 * @param pagetable the page table
 * @param va the virtual address
 * @return 0 if success, 1 if page not mapped
 */
int unmap_page(pagetable_t pagetable, uint64 va);

#endif // TOY_RISCV_KERNEL_KERNEL_VIRTUAL_MEMORY_H
