#ifndef TOY_RISCV_KERNEL_KERNEL_SWITCH_H
#define TOY_RISCV_KERNEL_KERNEL_SWITCH_H

#include "process.h"

void switch_context(struct context *old_context, struct context *new_context);

#endif // TOY_RISCV_KERNEL_KERNEL_SWITCH_H
