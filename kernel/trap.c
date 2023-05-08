#include "trap.h"

#include "kernel_vectors.h"
#include "memlayout.h"
#include "panic.h"
#include "process.h"
#include "riscv.h"
#include "trampoline.h"
#include "types.h"


enum cause { UNKNOWN, SYSCALL, TIMER };

enum cause supervisor_trap_cause() {
    uint64 scause = read_scause();

    switch (scause) {
        case 0x8000000000000001L: { // Supervisor software interrupt
            // ACTUALLY caused by TIMER interrupt
            // acknowledge the software interrupt by clearing
            // the SSIP bit in sip.
            write_sip(read_sip() & ~2);
            return TIMER;
        }
        case 8: { // Environment call from U-mode (ACTUALLY syscall)
            return SYSCALL;
        }
        default: {
            return UNKNOWN;
        }
    }
}

void user_trap() {
    uint64 sepc = read_sepc();
    uint64 sstatus = read_sstatus();
    enum cause trap_cause = supervisor_trap_cause();

    if ((sstatus & SSTATUS_SPP) != 0) {
        panic("user_trap: not from user mode");
    }
    
    // Send interrupts and exceptions to kernel_trap(), since we're now in
    // the kernel.
    write_stvec((uint64)kernel_vector);

    struct task_struct *task = current_task();
    
    // Save user process program counter.
    task->trap_frame->epc = sepc;
    
    switch (trap_cause) {
        case TIMER: {
            yield();
            break;
        }
        default:
            break;
    }

    user_trap_return();
}

void kernel_trap() {
    uint64 sepc = read_sepc();
    uint64 sstatus = read_sstatus();
    enum cause trap_cause = supervisor_trap_cause();

    if ((sstatus & SSTATUS_SPP) == 0) {
        panic("kernel_trap: not in supervisor mode");
    }
    if (interrupt_status() != 0) {
        panic("kernel_trap: interrupts enabled in supervisor mode");
    }

    switch (trap_cause) {
        case TIMER: {
            yield();
            break;
        }
        default:
            break;
    }

    // the yield() may have caused some traps to occur, so restore
    // trap registers for use by kernel_vector.S's sepc instruction.
    write_sepc(sepc);
    write_sstatus(sstatus);
}

void user_trap_return() {
    struct task_struct *task = current_task();

    // we're about to switch the destination of traps from
    // kernel_trap() to user_trap(), so turn off interrupts until
    // we're back in user space, where user_trap() is correct.
    interrupt_off();

    // send syscalls, interrupts, and exceptions to user_vector in trampoline.S
    uint64 trampoline_user_vector = TRAMPOLINE +
        ((uint64)user_vector - (uint64)trampoline);
    write_stvec(trampoline_user_vector);

    // set up trapframe values that user_vector will need when
    // the process next traps into the kernel.
    task->trap_frame->kernel_satp = read_satp();
    task->trap_frame->kernel_sp = (uint64)task->kernel_stack + PGSIZE;
    task->trap_frame->kernel_trap = (uint64)user_trap;

    // set up the registers that trampoline.S's sret will use
    // to get to user space.
    
    // set S Previous Privilege mode to User.
    unsigned long sstatus = read_sstatus();
    sstatus &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
    sstatus |= SSTATUS_SPIE; // enable interrupts in user mode
    write_sstatus(sstatus);

    // set S Exception Program Counter to the saved user pc.
    write_sepc(task->trap_frame->epc);

    // tell trampoline.S the user page table to switch to.
    uint64 satp = MAKE_SATP(task->pagetable);

    // jump to userret in trampoline.S at the top of memory, which 
    // switches to the user page table, restores user registers,
    // and switches to user mode with sret.
    uint64 trampoline_user_return = TRAMPOLINE +
        ((uint64)user_return - (uint64)trampoline);
    ((void (*)(uint64))trampoline_user_return)(satp);
}
