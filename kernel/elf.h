#ifndef TOY_RISCV_KERNEL_KERNEL_ELF_H
#define TOY_RISCV_KERNEL_KERNEL_ELF_H

#include <elf.h>

#include "process.h"

/**
 * Load the ELF file into the memory.
 * @param elf the ELF file
 * @param task the task_struct of the process
 * @return 0 if success, -1 if failed
 */
int load_elf(void *elf, struct task_struct *task);

#endif //TOY_RISCV_KERNEL_KERNEL_ELF_H
