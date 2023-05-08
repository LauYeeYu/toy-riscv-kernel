#include "mem_manage.h"
#include "print.h"
#include "process.h"
#include "riscv.h"
#include "test.h"
#include "types.h"
#include "uart.h"
#include "virtual_memory.h"

int main() {
    print_string("Switch to supervisor mode.\n");
    print_string("Initialize memory buddy system... ");
    init_mem_manage();
    print_string("Done.\n");
    print_string("Changing page table... ");
    init_kernel_pagetable();
    print_string("Done.\n");
    init_scheduler();
    test();
    return 0;
}
