// Mutual exclusion lock.
#include "types.h"

#ifndef TOY_RISCV_KERNEL_KERNEL_SPINLOCK_H
#define TOY_RISCV_KERNEL_KERNEL_SPINLOCK_H

typedef struct spinlock {
    uint locked;       // Is the lock held?

    // For debugging:
    char *name;        // Name of lock
} spinlock;

void initlock(spinlock *lk, char *name);
void acquire(spinlock *lk);
void release(spinlock *lk);
void push_off();
void pop_off();

#endif //TOY_RISCV_KERNEL_KERNEL_SPINLOCK_H
