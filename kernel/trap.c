#include "trap.h"

#include "kernel_vectors.h"
#include "memlayout.h"
#include "panic.h"
#include "plic.h"
#include "print.h"
#include "process.h"
#include "riscv.h"
#include "trampoline.h"
#include "types.h"
#include "uart.h"


enum cause {
    UNKNOWN,
    SYSCALL,
    TIMER,
    UART,
    VIRTIO,
    // user fault
    ILLEGAL_INSTRUCTION,
    INSTRUCTION_PAGE_FAULT,
    LOAD_PAGE_FAULT,
    STORE_PAGE_FAULT
};

enum cause supervisor_trap_cause() {
    uint64 scause = read_scause();

    if ((scause & 0x8000000000000000L) && (scause & 0xff) == 9) {
        // this is a supervisor external interrupt, via PLIC.

        // irq indicates which device interrupted.
        int irq = plic_claim();

        if (irq == UART0_IRQ) {
            uart_intr();
            plic_complete(irq);
            return UART;
        } else if (irq == VIRTIO0_IRQ) {
            // virtio_disk_intr();
            plic_complete(irq);
            return VIRTIO;
        } else if (irq) {
            print_string("unexpected interrupt irq=");
            print_int(irq, 10);
            print_string("\n");
            plic_complete(irq);
            return UNKNOWN;
        }
    }

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
        case 2: { // Illegal instruction
            return ILLEGAL_INSTRUCTION;
        }
        case 12: { // Instruction page fault
            return INSTRUCTION_PAGE_FAULT;
        }
        case 13: { // Load page fault
            return LOAD_PAGE_FAULT;
        }
        case 15: { // Store page fault
            return STORE_PAGE_FAULT;
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
        case SYSCALL: {
            syscall();
            break;
        }
        case ILLEGAL_INSTRUCTION: {
            print_string("Illegal instruction at ");
            print_int(read_stval(), 16);
            print_string(", pid ");
            print_int(task->pid, 10);
            print_string("\n");
            exit_process(task, -1);
        }
        case INSTRUCTION_PAGE_FAULT: {
            print_string("Instruction page fault at ");
            print_int(read_stval(), 16);
            print_string(", pid ");
            print_int(task->pid, 10);
            print_string("\n");
            exit_process(task, -1);
        }
        case LOAD_PAGE_FAULT: {
            print_string("Load page fault at ");
            print_int(read_stval(), 16);
            print_string(", pid ");
            print_int(task->pid, 10);
            print_string("\n");
            exit_process(task, -1);
        }
        case STORE_PAGE_FAULT: {
            print_string("Store page fault at ");
            print_int(read_stval(), 16);
            print_string(", pid ");
            print_int(task->pid, 10);
            print_string("\n");
            exit_process(task, -1);
        }
        default:
            break;
            //panic("user_trap: unexpected trap cause");
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
