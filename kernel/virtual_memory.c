#include "virtual_memory.h"

#include "mem_manage.h"
#include "panic.h"
#include "riscv_defs.h"
#include "types.h"
#include "riscv.h"
#include "utility.h"

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
    return (size_t)pages * PGSIZE;
}

void free_memory(pagetable_t pagetable, uint64 start, size_t size) {
    start = PGROUNDDOWN(start);
    for (uint64 i = 0; i < size; i += PGSIZE) {
        deallocate((void *)physical_address(pagetable, start + i), 0);
        unmap_page(pagetable, start + i);
    }
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
