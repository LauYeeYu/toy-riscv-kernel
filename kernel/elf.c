#include "elf.h"

#include <elf.h>
#include "print.h"
#include "process.h"
#include "riscv.h"
#include "riscv_defs.h"
#include "virtual_memory.h"
#include "utility.h"

int check_elf_magic(uint8 *elf) {
    if (elf[EI_MAG0] != ELFMAG0 ||
        elf[EI_MAG1] != ELFMAG1 ||
        elf[EI_MAG2] != ELFMAG2 ||
        elf[EI_MAG3] != ELFMAG3) {
        return -1;
    }
    return 0;
}

int load_elf(void *elf, struct task_struct *task) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf;
    //check magic number
    if (check_elf_magic(elf) != 0) return -1;

    Elf64_Phdr *phdr = (Elf64_Phdr *)((uint64)elf + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++)
    {
        if (phdr[i].p_type == PT_GNU_STACK) task->stack_permission |= PTE_X;
        if (phdr[i].p_type != PT_LOAD) continue;

        void *src_addr = (void *)((uint64)elf + phdr[i].p_offset);
        uint64 va = phdr[i].p_vaddr;
        size_t size = phdr[i].p_memsz;
        size_t src_size = phdr[i].p_filesz;
        size_t section_start = PGROUNDDOWN(va);
        size_t section_size = PGROUNDUP(va + size) - section_start;

        // flags
        uint64 permission = PTE_U;
        if (phdr[i].p_flags & PF_R) permission |= PTE_R;
        if (phdr[i].p_flags & PF_W) permission |= PTE_W;
        if (phdr[i].p_flags & PF_X) permission |= PTE_X;
        
        if (map_section_for_user(task->pagetable, va,
                                 src_addr, src_size, size,
                                 permission) != 0) {
            return -1;
        }
        if (register_memory_section(task, section_start, section_size)) {
            free_memory(task->pagetable, va, section_size);
            return -1;
        }
    }

    task->trap_frame->epc = ehdr->e_entry;
    return 0;
}
