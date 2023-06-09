#include "print.h"
#include "riscv.h"

volatile int panicked = 0;

__attribute__((noreturn)) void panic(const char *msg) {
    interrupt_off();
    print_string("panic: ");
    print_string(msg);
    print_string("\n");
    for (;;) {}
}
