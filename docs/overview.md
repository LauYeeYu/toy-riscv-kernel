# RISCV Kernel

## Start Up

Set up the environment for the kernel.

Before entering a C function, the start-up procedure will set up the stack
before jump to a C function ([entry.S](../kernel/entry.S)).

Then the program will enter the `start` function in
[start.c](../kernel/start.c). It will

- initialize the UART for early output;
- set up a identically-mapping page table;
- set up things for interrupt;
- change the mode to supervisor mode and jump to the `main` function
  in [main.c](../kernel/main.c).

See [start_up.md](start_up.md).

## Memory Management

The kernel uses buddy system to manage memory.

## Process Management

See [process.md](process.md).

For system calls, see [system_call.md](system_call.md).
