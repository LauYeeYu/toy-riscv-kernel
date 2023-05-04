#ifndef TOY_RISCV_KERNEL_KERNEL_TRAP_H
#define TOY_RISCV_KERNEL_KERNEL_TRAP_H

#include "types.h"

/**
 * Handle all kinds of traps in user mode. Interrupts and exceptions from user
 * code go here via user_vector.
 */
void user_trap();

/**
 * Handle all kinds of traps in supervisor mode. Interrupts and exceptions
 * from kernel code go here via kernel_vector, on whatever the current kernel
 * stack is.
 */
void kernel_trap();

void user_trap_return();

#endif //TOY_RISCV_KERNEL_KERNEL_TRAP_H
