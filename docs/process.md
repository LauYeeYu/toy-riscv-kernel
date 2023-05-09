# Process Management

For every user process, it needs to run as if it is the only process.
Therefore, we need a mechanism to isolate different processes. Every process
has their own resources, including memory, page table, etc. These things are
stored and managed in a structure called `task_struct` (defined in
[process.h](../kernel/process.h)).

On top of that, user processes need to trap into kernel for several reasons -
system calls, timer interrupts, page faults, etc. Therefore, we need to set up
a mechanism to switch between user mode and kernel mode.

To make this work, we need to set up a [trampoline](#trampoline).

Further more, we need to set up a mechanism to switch between different
processes with time slicing - a [scheduler](#scheduler).

## Trampoline

Obviously, the kernel and the user process cannot share the same page table -
this will expose everything in kernel to the user process. Therefore, the user
processes will have their own dedicated page tables.

However, the program counter (PC) register cannot jump to the place where user
processes start directly. Then, the idea of trampoline appears.

Normally, a trampoline is a share memory both in user and kernel states with
absolutely the same address. Every time we need to switch between user and
kernel, we need to jump to the trampoline space, and then jump again to the
place we want, really like a trampoline.

In this design, the trampoline is put at the highest possible virtual memory
space, i.e., the highest virtual memory page.

The trampoline is implemented in [trampoline.S](../kernel/trampoline.S). It
has two functions:
- `user_vector`: handling all traps in user mode;
- `user_return`: return from user mode to kernel mode.

## Traps

Every time a user process traps into kernel, we need to save the context in
trap frame (with [trampoline](#trampoline)), and then check the cause and call
the corresponding function to handle these traps.

## Scheduler

Normally, a user process will be switched out of CPU for two reasons:

- The process is blocked by some system calls;
- The process has used up its time slice.

For the former case, see the [document for system calls](system_call.md)
for more details.

For the latter case, we uses the timer to slice the time. The complete
procedure is - set the timer, wait for the timer interrupt, arrange for a
supervisor software interrupt, and then switch the process when handling
that software interrupt. The timer part is done in `timer_vector` in
[kernel_vectors.S](../kernel/kernel_vectors.S); the software interrupt part is
done `trap.c`, and then the context is switched in
[process.c](../kernel/process.c).
