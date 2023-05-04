#ifndef TOY_RISCV_KERNEL_KERNEL_KERNEL_VECTORS_H
#define TOY_RISCV_KERNEL_KERNEL_KERNEL_VECTORS_H

/* The vector for supervisor interrupt and exceptions. */
void kernel_vector();

/* The vector for timer interrupt. */
void timer_vector();

#endif //TOY_RISCV_KERNEL_KERNEL_KERNEL_VECTORS_H
