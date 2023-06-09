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

#include "process.h"
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
 * Change the pagetable to be a finer one in terms of its permission control.
 */
void init_kernel_pagetable();

/**
 * Create a void page table. If there is no memory, a NULL pointer is returned.
 */
pagetable_t create_void_pagetable();

/**
 * Map the memory from source to [start, start + size) in the page table.
 * @param pagetable the page table
 * @param src the source address
 * @param size the size of the memory
 * @return the actual size of memory used
 */
size_t map_memory(pagetable_t pagetable,
                  void *src,
                  size_t size,
                  uint64 permission);

/**
 * Copy the memory from [start, start + size) to the target_pagetable.
 * @param source_pagetable the source page table
 * @param target_pagetable the target page table
 * @param va_start the start virtual address
 * @param size the size of the data
 * @return 0 if succeeded, -1 if failed
 */
int copy_memory_with_pagetable(pagetable_t source_pagetable,
                               pagetable_t target_pagetable,
                               uint64 va_start,
                               uint64 size);

/**
 * Copy all the memory in mem_sections from source to target.
 * @param source the source task
 * @param target the target task
 * @return 0 if succeeded, -1 if failed
 */
int copy_all_memory_with_pagetable(struct task_struct *source,
                                   struct task_struct *target);
/**
 * Free the memory from [start, start + size).
 * @param pagetable the page table
 * @param start the start address
 * @param size the size of the memory
 */
void free_memory(pagetable_t pagetable, uint64 start, size_t size);

/**
 * Free the page table. Please note that is function will not free the pages
 * since freeing some static pages (like trampoline) is problematic.
 * @param pagetable the page table
 */
void free_pagetable(pagetable_t pagetable);

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

/**
 * Map the page from src to a page at pa in page table for user.
 * The va and size do not need to be page aligned.
 * @param pagetable the page table
 * @param va the virtual address
 * @param src the source address
 * @param size the size of the memory
 * @param permission the permission of the page, see riscv_defs.h for details
 */
int map_page_for_user(pagetable_t pagetable,
                      uint64 va,
                      void *src,
                      size_t size,
                      uint64 permission);

/**
 * Map the section from src to a section at va_start in page table for user.
 * The va_start and size do not need to be page aligned.
 * @param pagetable the page table
 * @param va_start the start virtual address
 * @param src the source address
 * @param size the size of the memory
 * @param permission the permission of the page, see riscv_defs.h for details
 */
int map_section_for_user(pagetable_t pagetable,
                         uint64 va_start,
                         void *src,
                         size_t src_size,
                         size_t size,
                         uint64 permission);

#endif // TOY_RISCV_KERNEL_KERNEL_VIRTUAL_MEMORY_H
