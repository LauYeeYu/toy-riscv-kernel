#include "virtual_memory.h"

#include "mem_manage.h"
#include "memlayout.h"
#include "panic.h"
#include "process.h"
#include "riscv_defs.h"
#include "riscv.h"
#include "single_linked_list.h"
#include "types.h"
#include "utility.h"

extern char etext[];  // kernel.ld sets this to end of kernel code.
extern char trampoline[]; // trampoline.S

// A finer page table for kernel, initialized in make_kernel_pagetable
pagetable_t kernel_pagetable = NULL;

pte_t *pagetable_entry(pagetable_t pagetable, uint64 va, int alloc) {
    if (va >= MAXVA) panic("pagetable_entry: va >= MAXVA");

    for (int level = 2; level > 0; level--) {
        pte_t *pte = &pagetable[PX(level, va)];
        if (*pte & PTE_V) {
            pagetable = (pte_t *)PTE2PA(*pte);
        } else {
            // That entry doesn't exist yet.
            if (!alloc || (pagetable = (pte_t *)allocate(1)) == 0) {
                return NULL;
            }
            memset(pagetable, 0, PGSIZE);
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    return &pagetable[PX(0, va)];
}

uint64 physical_address(pagetable_t pagetable, uint64 va) {
    pte_t *pte = pagetable_entry(pagetable, va, 0);
    if (pte == NULL || (*pte & PTE_V) == 0) return NULL;
    return PTE2PA(*pte) + PGOFFSET(va);
}

void kernel_map_pages(pagetable_t pagetable,
                      uint64 va,
                      uint64 pa,
                      uint64 size,
                      uint64 permission) {
    va = PGROUNDDOWN(va);
    pa = PGROUNDDOWN(pa);
    for (uint64 i = 0; i < size; i += PGSIZE) {
        int result = map_page(pagetable, va + i, pa + i, permission);
        if (result == -1) panic("kernel_map_pages: map_page failed");
    }
}

void make_kernel_pagetable() {
    kernel_pagetable = allocate(0); // Get a page for the root level
    if (kernel_pagetable == NULL) {
        panic("make_kernel_pagetable: page table allocate failed");
    }
    memset(kernel_pagetable, 0, PGSIZE); // Need to set all entries as invalid

    // Map components of kernel
    uint64 rw = PTE_R | PTE_W;
    uint64 rx = PTE_R | PTE_X;
    // UART
    kernel_map_pages(kernel_pagetable, UART0, UART0, PGSIZE, rw);
    // virtio mmio disk interface
    kernel_map_pages(kernel_pagetable, VIRTIO0, VIRTIO0, PGSIZE, rw);
    // PLIC
    kernel_map_pages(kernel_pagetable, PLIC, PLIC, 0x400000, rw);
    // map kernel text executable and read-only.
    kernel_map_pages(kernel_pagetable, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, rx);
    // map kernel data and the physical RAM we'll make use of.
    kernel_map_pages(kernel_pagetable, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, rw);
    // map the trampoline for trap entry/exit to the highest virtual
    // address in the kernel.
    kernel_map_pages(kernel_pagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, rx);
}

void init_kernel_pagetable() {
    // make the kernel pagetable
    make_kernel_pagetable();

    // wait for any previous writes to the page table memory to finish.
    sfence_vma();

    write_satp(MAKE_SATP(kernel_pagetable));

    // flush stale entries from the TLB.
    sfence_vma();
}

pagetable_t create_void_pagetable() {
    pagetable_t pagetable = (pagetable_t)allocate(1);
    if (pagetable == NULL) return NULL;
    memset(pagetable, 0, PGSIZE);
    return pagetable;
}

inline uint64 power_of_pages(size_t size) {
    uint64 power = 0;
    while ((PGSIZE << power) < size) power++;
    return power;
}

size_t map_memory(pagetable_t pagetable,
                  void *src,
                  size_t size,
                  uint64 permission) {
    // allocate pages
    uint64 power = power_of_pages(size);
    void *pages = allocate(power);
    uint64 number_of_pages = 1 << power;

    // copy the data from src to pages
    memcpy(pages, src, size);

    // map the pages to the pagetable
    for (uint64 i = 0; i < number_of_pages; i++) {
        int result = map_page(
            pagetable,
            i * PGSIZE,
            (uint64)pages + i * PGSIZE,
            permission
        );
        if (result == -1) {
            for (uint64 j = 0; j < i; j++) {
                unmap_page(pagetable, j * PGSIZE);
            }
            deallocate(pages, power);
            return 0;
        }
    }
    return PGSIZE << power;
}

int copy_memory_with_pagetable(pagetable_t source_pagetable,
                               pagetable_t target_pagetable,
                               uint64 va_start,
                               uint64 size) {
    if (va_start & (PGSIZE - 1) || size & (PGSIZE - 1)) {
        panic("copy_memory_with_pagetable: va_start or size is not page aligned");
    }
    if (va_start + size >= MAXVA) {
        panic("copy_memory_with_pagetable: va_start + size >= MAXVA");
    }
    for (uint64 offset = 0; offset < size; offset += PGSIZE) {
        uint64 va = va_start + offset;
        void *src = (void *)physical_address(source_pagetable, va);
        void *dest = allocate(0);
        if (src == NULL) {
            panic("copy_memory_with_pagetable: memory not mapped");
        }
        if (dest == NULL) {
            for (uint64 offset2 = 0; offset2 < offset; offset2 += PGSIZE) {
                void *src2 = (void *)physical_address(source_pagetable, va_start + offset2);
                unmap_page(target_pagetable, va_start + offset2);
                deallocate(src2, 0);
            }
            return -1;
        }
        memcpy(dest, src, PGSIZE);
        int result = map_page(
            target_pagetable,
            va,
            (uint64)dest,
            PTE_FLAGS(*pagetable_entry(source_pagetable, va, 0))
        );
        if (result == -1) {
            for (uint64 offset2 = 0; offset2 < offset; offset2 += PGSIZE) {
                void *src2 = (void *)physical_address(source_pagetable, va_start + offset2);
                unmap_page(target_pagetable, va_start + offset2);
                deallocate(src2, 0);
            }
            deallocate(dest, 0);
            return -1;
        }
    }
    return 0;
}

int copy_all_memory_with_pagetable(struct task_struct *source,
                                   struct task_struct *target) {
    for (struct single_linked_list_node *node = head_node(&(source->mem_sections));
        node != NULL;
        node = node->next) {
        struct memory_section *mem_section = node->data;
        if (copy_memory_with_pagetable(
            source->pagetable,
            target->pagetable,
            mem_section->start,
            mem_section->size
        ) != 0) {
            for (struct single_linked_list_node *node2 = head_node(&(target->mem_sections));
                node2 != NULL;
                node2 = node2->next) {
                struct memory_section *mem_section2 = node2->data;
                for (uint64 offset = 0; offset < mem_section2->size; offset += PGSIZE) {
                    deallocate(
                        (void *)physical_address(
                            target->pagetable,
                            (uint64)mem_section2->start + offset
                        ),
                        0);
                    unmap_page(target->pagetable, (uint64)mem_section2->start + offset);
                }
            }
            return -1;
        }
    }
    if (copy_memory_with_pagetable(source->pagetable, target->pagetable,
                                   source->stack.start, source->stack.size) != 0) {
        for (struct single_linked_list_node *node2 = head_node(&(target->mem_sections));
            node2 != NULL;
            node2 = node2->next) {
            struct memory_section *mem_section2 = node2->data;
            for (uint64 offset = 0; offset < mem_section2->size; offset += PGSIZE) {
                deallocate(
                    (void *)physical_address(
                        target->pagetable,
                        (uint64)mem_section2->start + offset
                    ),
                    0);
                unmap_page(target->pagetable, (uint64)mem_section2->start + offset);
            }
        }
    }
    return 0;
}

void free_memory(pagetable_t pagetable, uint64 start, size_t size) {
    start = PGROUNDDOWN(start);
    for (uint64 i = 0; i < size; i += PGSIZE) {
        deallocate((void *)physical_address(pagetable, start + i), 0);
        unmap_page(pagetable, start + i);
    }
}

void free_pagetable_internal(pagetable_t pagetable, int level) {
    if (level > 0) {
        for (uint64 i = 0; i < 512; i++) {
            pte_t *pte = &pagetable[i];
            if ((*pte & PTE_V) && level > 1) {
                free_pagetable_internal((pagetable_t)PTE2PA(*pte), level - 1);
            }
        }
    }
    deallocate(pagetable, 0);
}

void free_pagetable(pagetable_t pagetable) {
    free_pagetable_internal(pagetable, 2);
}

int map_page(pagetable_t pagetable,
             uint64 va,
             uint64 pa,
             uint64 permission) {
    va = PGROUNDDOWN(va);
    pa = PGROUNDDOWN(pa);
    pte_t *pte = pagetable_entry(pagetable, va, 1);

    if (pte == NULL) return 1;
    if (*pte & PTE_V) panic("map_page: page already mapped");

    *pte = PA2PTE(pa) | permission | PTE_V;
    return 0;
}

int unmap_page(pagetable_t pagetable, uint64 va) {
    va = PGROUNDDOWN(va);
    pte_t *pte = pagetable_entry(pagetable, va, 0);

    if (pte == NULL) return 1;
    if ((*pte & PTE_V) == 0) panic("unmap_page: page not mapped");
    if(PTE_FLAGS(*pte) == PTE_V) panic("unmap_page: cannot unmap leaf page");

    *pte = 0;
    return 0;
}

int map_page_for_user(pagetable_t pagetable,
                      uint64 va,
                      void *src,
                      size_t size,
                      uint64 permission) {
    void *page = allocate_for_user(0);
    if (page == NULL) return -1;
    memcpy(page, src, size);
    if (map_page(pagetable, va, (uint64)page, permission) != 0) {
        deallocate(page, 0);
        return -1;
    }
    return 0;
}

int map_section_for_user(pagetable_t pagetable,
                         uint64 va_start,
                         void *src,
                         size_t size,
                         uint64 permission) {
    uint64 va_end = va_start + size;
    uint8 *src_addr = (uint8 *)src;
    uint64 page_start = PGROUNDDOWN(va_start);
    for (uint64 page_addr = page_start;
         page_addr < va_end;
         page_addr += PGSIZE) {
        uint64 dest_start = max(va_start, page_addr);
        uint64 dest_end = min(va_end, page_addr + PGSIZE);
        if (map_page_for_user(pagetable,
                              dest_start,
                              src_addr,
                              dest_end - dest_start,
                              permission) != 0) {
            free_memory(pagetable, page_start, page_addr - page_start);
            return -1;
        }
        src_addr += dest_end - dest_start;
    }
    return 0;
}
