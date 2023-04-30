/* 
 * Tools for RISC-V
 * includes:
 *   Getters and Setters for the CSR registers
 *   Core information for privileged ISA
 */

#ifndef TOY_RISCV_KERNEL_KERNEL_RISCV_H
#define TOY_RISCV_KERNEL_KERNEL_RISCV_H


#include "types.h"
#include "riscv_defs.h"

static inline uint64 read_mstatus() {
    uint64 x;
    asm volatile("csrr %0, mstatus" : "=r" (x) );
    return x;
}

static inline void write_mstatus(uint64 x) {
    asm volatile("csrw mstatus, %0" : : "r" (x));
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void write_mepc(uint64 x) {
    asm volatile("csrw mepc, %0" : : "r" (x));
}

static inline uint64 read_sstatus() {
    uint64 x;
    asm volatile("csrr %0, sstatus" : "=r" (x) );
    return x;
}

static inline void write_sstatus(uint64 x) {
    asm volatile("csrw sstatus, %0" : : "r" (x));
}

// Supervisor Interrupt Pending
static inline uint64 read_sip() {
    uint64 x;
    asm volatile("csrr %0, sip" : "=r" (x) );
    return x;
}

static inline void write_sip(uint64 x) {
    asm volatile("csrw sip, %0" : : "r" (x));
}

static inline uint64 read_sie() {
    uint64 x;
    asm volatile("csrr %0, sie" : "=r" (x) );
    return x;
}

static inline void write_sie(uint64 x) {
    asm volatile("csrw sie, %0" : : "r" (x));
}

static inline uint64 read_mie() {
    uint64 x;
    asm volatile("csrr %0, mie" : "=r" (x) );
    return x;
}

static inline void write_mie(uint64 x) {
    asm volatile("csrw mie, %0" : : "r" (x));
}

// supervisor exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void write_sepc(uint64 x) {
    asm volatile("csrw sepc, %0" : : "r" (x));
}

static inline uint64 read_sepc() {
    uint64 x;
    asm volatile("csrr %0, sepc" : "=r" (x) );
    return x;
}

// Machine Exception Delegation
static inline uint64 read_medeleg() {
    uint64 x;
    asm volatile("csrr %0, medeleg" : "=r" (x) );
    return x;
}

static inline void write_medeleg(uint64 x) {
    asm volatile("csrw medeleg, %0" : : "r" (x));
}

// Machine Interrupt Delegation
static inline uint64 read_mideleg() {
    uint64 x;
    asm volatile("csrr %0, mideleg" : "=r" (x) );
    return x;
}

static inline void write_mideleg(uint64 x) {
    asm volatile("csrw mideleg, %0" : : "r" (x));
}

// Supervisor Trap-Vector Base Address
// low two bits are mode.
static inline void write_stvec(uint64 x) {
   asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline uint64 read_stvec() {
    uint64 x;
    asm volatile("csrr %0, stvec" : "=r" (x) );
    return x;
}

// Machine-mode interrupt vector
static inline void write_mtvec(uint64 x) {
    asm volatile("csrw mtvec, %0" : : "r" (x));
}

// Physical Memory Protection
static inline void write_pmpcfg0(uint64 x) {
    asm volatile("csrw pmpcfg0, %0" : : "r" (x));
}

static inline void write_pmpaddr0(uint64 x) {
    asm volatile("csrw pmpaddr0, %0" : : "r" (x));
}

// supervisor address translation and protection;
// holds the address of the page table.
static inline void write_satp(uint64 x) {
    asm volatile("csrw satp, %0" : : "r" (x));
}

static inline uint64 read_satp() {
    uint64 x;
    asm volatile("csrr %0, satp" : "=r" (x) );
    return x;
}

static inline void write_mscratch(uint64 x) {
    asm volatile("csrw mscratch, %0" : : "r" (x));
}

// Supervisor Trap Cause
static inline uint64 read_scause() {
    uint64 x;
    asm volatile("csrr %0, scause" : "=r" (x) );
    return x;
}

// Supervisor Trap Value
static inline uint64 read_stval() {
    uint64 x;
    asm volatile("csrr %0, stval" : "=r" (x) );
    return x;
}

// Machine-mode Counter-Enable
static inline void write_mcounteren(uint64 x) {
    asm volatile("csrw mcounteren, %0" : : "r" (x));
}

static inline uint64 read_mcounteren() {
    uint64 x;
    asm volatile("csrr %0, mcounteren" : "=r" (x) );
    return x;
}

// machine-mode cycle counter
static inline uint64 read_time() {
    uint64 x;
    asm volatile("csrr %0, time" : "=r" (x) );
    return x;
}

// enable device interrupts
static inline void interrupt_on() {
    write_sstatus(read_sstatus() | SSTATUS_SIE);
}

// disable device interrupts
static inline void interrupt_off() {
    write_sstatus(read_sstatus() & ~SSTATUS_SIE);
}

// are device interrupts enabled?
static inline int interrupt_status() {
    uint64 x = read_sstatus();
    return (x & SSTATUS_SIE) != 0;
}

// Set the interrupt status and return the old status.
static inline int set_interrupt_status(int status) {
    int last_status = interrupt_status();
    if (status) interrupt_on();
    else interrupt_off();
    return last_status;
}

static inline uint64 read_sp() {
    uint64 x;
    asm volatile("mv %0, sp" : "=r" (x) );
    return x;
}

// read and write tp, the thread pointer, which xv6 uses to hold
// this core's hartid (core number), the index into cpus[].
static inline uint64 read_tp() {
    uint64 x;
    asm volatile("mv %0, tp" : "=r" (x) );
    return x;
}

static inline void write_tp(uint64 x) {
    asm volatile("mv tp, %0" : : "r" (x));
}

static inline uint64 read_ra() {
    uint64 x;
    asm volatile("mv %0, ra" : "=r" (x) );
    return x;
}

// flush the TLB.
static inline void sfence_vma()
{
    // the zero, zero means flush all TLB entries.
    asm volatile("sfence.vma zero, zero");
}

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs

#endif // TOY_RISCV_KERNEL_KERNEL_RISCV_H
