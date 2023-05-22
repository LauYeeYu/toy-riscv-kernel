#ifndef TOY_RISCV_KERNEL_KERNEL_PRINT_H
#define TOY_RISCV_KERNEL_KERNEL_PRINT_H

#include "types.h"

void print_char(char c);
void print_string(const char *str);
void print_int(uint64 x, int base);

#endif //TOY_RISCV_KERNEL_KERNEL_PRINT_H
