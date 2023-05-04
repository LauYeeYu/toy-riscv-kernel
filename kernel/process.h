#ifndef TOY_RISCV_KERNEL_KERNEL_PROC_H
#define TOY_RISCV_KERNEL_KERNEL_PROC_H

#include "types.h"

enum process_state { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Saved registers for kernel context switches.
struct context {
    uint64 ra;
    uint64 sp;

    // callee-saved
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
};

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// user_vector in trampoline.S saves user registers in the trap_frame,
// then initializes registers from the trap_frame's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
struct trap_frame {
    /*   0 */ uint64 kernel_satp;   // kernel page table
    /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
    /*  16 */ uint64 kernel_trap;   // user_trap()
    /*  24 */ uint64 epc;           // saved user program counter
    /*  32 */ uint64 kernel_hartid; // saved kernel tp
    /*  40 */ uint64 ra;
    /*  48 */ uint64 sp;
    /*  56 */ uint64 gp;
    /*  64 */ uint64 tp;
    /*  72 */ uint64 t0;
    /*  80 */ uint64 t1;
    /*  88 */ uint64 t2;
    /*  96 */ uint64 s0;
    /* 104 */ uint64 s1;
    /* 112 */ uint64 a0;
    /* 120 */ uint64 a1;
    /* 128 */ uint64 a2;
    /* 136 */ uint64 a3;
    /* 144 */ uint64 a4;
    /* 152 */ uint64 a5;
    /* 160 */ uint64 a6;
    /* 168 */ uint64 a7;
    /* 176 */ uint64 s2;
    /* 184 */ uint64 s3;
    /* 192 */ uint64 s4;
    /* 200 */ uint64 s5;
    /* 208 */ uint64 s6;
    /* 216 */ uint64 s7;
    /* 224 */ uint64 s8;
    /* 232 */ uint64 s9;
    /* 240 */ uint64 s10;
    /* 248 */ uint64 s11;
    /* 256 */ uint64 t3;
    /* 264 */ uint64 t4;
    /* 272 */ uint64 t5;
    /* 280 */ uint64 t6;
};

// Per-process state
struct task_struct {
    // if multiple CPUs are supported, there should be a spinlock here
    enum process_state state;      // Process state
    pid_t pid;                     // Process ID
    struct task_struct *parent;    // Parent process
    void *kernel_stack;            // Virtual address of kernel stack
    uint64 sz;                     // Size of process memory (bytes)
    pte_t *pagetable;              // User page table
    struct trap_frame *trap_frame; // data page for trampoline.S
    struct context context;        // swtch() here to run process
    char name[32];                 // Process name (debugging)
};

struct task_struct *current_task();

/**
 * Initialize the scheduler.
 * Set up the environment for the scheduler, and create the first user process.
 */
void init_scheduler();

/* Start the scheduling work. */
void scheduler();

/* Try to switch to other user process. */
void yield();

#endif //TOY_RISCV_KERNEL_KERNEL_PROC_H