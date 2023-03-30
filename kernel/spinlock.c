// Mutual exclusion spin locks.
#include "spinlock.h"

#include "types.h"

void push_off();
void pop_off();

void initlock(spinlock *lk, char *name) {
    lk->name = name;
    lk->locked = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void acquire(spinlock *lk) {
    push_off(); // disable interrupts to avoid deadlock.

    while(lk->locked != 0) {}

    // Tell the C compiler and the processor to not move loads or stores
    // past this point, to ensure that the critical section's memory
    // references happen strictly after the lock is acquired.
    // On RISC-V, this emits a fence instruction.
    __sync_synchronize();
}

// Release the lock.
void release(spinlock *lk) {
    // Tell the C compiler and the CPU to not move loads or stores
    // past this point, to ensure that all the stores in the critical
    // section are visible to other CPUs before the lock is released,
    // and that loads in the critical section occur strictly before
    // the lock is released.
    // On RISC-V, this emits a fence instruction.
    __sync_synchronize();

    // Release the lock, equivalent to lk->locked = 0.
    // This code doesn't use a C assignment, since the C standard
    // implies that an assignment might be implemented with
    // multiple store instructions.
    // On RISC-V, sync_lock_release turns into an atomic swap:
    //   s1 = &lk->locked
    //   amoswap.w zero, zero, (s1)
    __sync_lock_release(&lk->locked);

    pop_off();
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.
void push_off() {
    // FIXME: make this code run as expected
    //int old = intr_get();

    //intr_off();
    //if(mycpu()->noff == 0)
    //    mycpu()->intena = old;
    //mycpu()->noff += 1;
    //
}

void pop_off() {
    /* FIXME: make this code run as expected
    struct cpu *c = mycpu();
    if(intr_get())
        panic("pop_off - interruptible");
    if(c->noff < 1)
        panic("pop_off");
    c->noff -= 1;
    if(c->noff == 0 && c->intena)
        intr_on();
    */
}
