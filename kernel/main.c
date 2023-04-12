#include "mem_manage.h"
#include "print.h"
#include "riscv.h"
#include "types.h"
#include "uart.h"

int main() {
    print_string("Switch to supervisor mode.\n");
    print_string("Initialize memory buddy system...");
    init_mem_manage();
    print_string("Done.\n");
    return 0;
}