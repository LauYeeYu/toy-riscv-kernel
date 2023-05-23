#include "virtual_memory.h"

#include "mem_manage.h"
#include "memlayout.h"
#include "panic.h"
#include "riscv_defs.h"
#include "riscv.h"
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

inline uint64 power_of_pages(uint64 size) {
    size_t power = 0;
    while ((PGSIZE << power) < size) power++;
    return power;
}

size_t init_virtual_memory_for_user(pagetable_t pagetable,
                                    void *src,
                                    size_t size) {
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
            PTE_R | PTE_W | PTE_X | PTE_U
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

size_t init_pagetable_for_user(pagetable_t pagetable,
                               void *data,
                               size_t size) {
    if ((uint64)data & (PGSIZE - 1) || size & (PGSIZE - 1)) {
        panic("init_pagetable_for_user: data not aligned");
    }

    // map the pages to the pagetable
    for (uint64 i = 0; i < size; i += PGSIZE) {
        int result = map_page(
            pagetable,
            i,
            (uint64)data + i,
            PTE_R | PTE_W | PTE_X | PTE_U
        );
        if (result == -1) {
            for (uint64 j = 0; j < i; j += 4096) {
                unmap_page(pagetable, j);
            }
            return -1;
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
