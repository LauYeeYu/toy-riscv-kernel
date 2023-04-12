// Mutual exclusion lock.
#include "types.h"

#ifndef TOY_RISCV_KERNEL_KERNEL_SPINLOCK_H
#define TOY_RISCV_KERNEL_KERNEL_SPINLOCK_H

typedef struct spinlock {
    uint locked;       // Is the lock held?

    // For debugging:
    char *name;        // Name of lock
} spinlock;

inline void initlock(spinlock *lk, char *name) {}
inline void acquire(spinlock *lk) {}
inline void release(spinlock *lk) {}
inline void push_off() {}
inline void pop_off() {}

#endif //TOY_RISCV_KERNEL_KERNEL_SPINLOCK_H
