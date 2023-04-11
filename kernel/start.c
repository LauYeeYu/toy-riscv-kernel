#include "print.h"
#include "riscv.h"
#include "types.h"
#include "uart.h"

pte_t kernel_pagetable[512] __attribute__((aligned(4096)));

int main();
void establish_page_table();

// entry.S jumps here in machine mode.
void start() {
    uart_init();
    print_string("Entering kernel...\n");

    // Set up an identically-mapping page table. (all huge pages)
    print_string("Establishing page table... ");
    establish_page_table();
    print_string("Done.\n");

    print_string("Setting mstatus... ");
    unsigned long x = read_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    write_mstatus(x);
    print_string("Done.\n");

    print_string("Setting things for interruption... ");
    write_medeleg(0xffff);
    write_mideleg(0xffff);
    write_sie(read_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
    print_string("Done.\n");

    // configure Physical Memory Protection to give supervisor mode
    // access to all of physical memory.
    print_string("Setting PMP... ");
    write_pmpaddr0(0x3fffffffffffffull);
    write_pmpcfg0(0xf);
    print_string("Done.\n");

    print_string("Enabling paging... ");
    // Set the page table.
    write_satp(((uint64)kernel_pagetable >> 12) | (8ull << 60));
    // Enable paging.
    write_sstatus(read_sstatus() | SSTATUS_SPP | SSTATUS_SIE);
    print_string("Done.\n");

    write_mepc((uint64)main);
    // enter supervisor mode, and jump to main().
    asm volatile("mret");
}

/*
 * Establish a page table that maps the entire physical memory to the
 * virtual memory.
 *
 * page table entry:
 * 63 62 61 60   54 53  28 27  19 18  10 9 8 7 6 5 4 3 2 1 0
 * +-+----+--------+------+------+------+---+-+-+-+-+-+-+-+-+
 * |N|PBMT|reserved|PPN[2]|PPN[1]|PPN[0]|RSW|D|A|G|U|X|W|R|V|
 * +-+----+--------+------+------+------+---+-+-+-+-+-+-+-+-+
 */
void establish_page_table() {
    for (uint64 i = 0; i < 512; i++) {
        pte_t pte = 0;
        pte |= (i << 28); // PPN[2]
        // a valid read-write-executable page
        pte |= 0b01111;
        kernel_pagetable[i] = pte;
    }
}
