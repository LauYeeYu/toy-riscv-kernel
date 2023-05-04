#ifndef TOY_RISCV_KERNEL_KERNEL_PANIC_H
#define TOY_RISCV_KERNEL_KERNEL_PANIC_H

extern int panicked;

__attribute__((noreturn)) void panic(const char *msg);

#endif //TOY_RISCV_KERNEL_KERNEL_PANIC_H
