#include "elf.h"

#include <elf.h>

#include "process.h"
#include "riscv.h"
#include "riscv_defs.h"
#include "virtual_memory.h"
#include "utility.h"

int load_elf(void *elf, struct task_struct *task) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf;
    Elf64_Phdr *phdr = (Elf64_Phdr *)((uint64)elf + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++)
    {
        if (phdr[i].p_type == PT_GNU_STACK) task->stack_permission |= PTE_X;
        if (phdr[i].p_type != PT_LOAD) continue;

        void *src_addr = (void *)((uint64)elf + phdr[i].p_offset);
        uint64 va = phdr[i].p_vaddr;

        // flags
        uint64 permission = PTE_U;
        if (phdr[i].p_flags & PF_R) permission |= PTE_R;
        if (phdr[i].p_flags & PF_W) permission |= PTE_W;
        if (phdr[i].p_flags & PF_X) permission |= PTE_X;
        
        if (map_section_for_user(task->pagetable, va,
                                 src_addr, phdr[i].p_filesz,
                                 permission) != 0) {
            return -1;
        } 
    }
    return 0;
}
