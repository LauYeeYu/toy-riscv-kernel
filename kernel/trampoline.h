#ifndef TOY_RISCV_KERNEL_KERNEL_TRAMPOLINE_H
#define TOY_RISCV_KERNEL_KERNEL_TRAMPOLINE_H

#include "types.h"

void trampoline();
void user_vector();
void user_return(uint64 pagetable);

#endif //TOY_RISCV_KERNEL_KERNEL_TRAMPOLINE_H
