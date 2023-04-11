#include "print.h"
#include "riscv.h"
#include "types.h"
#include "uart.h"

int main() {
    print_string("Switch to supervisor mode.\n");
    return 0;
}