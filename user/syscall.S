    .section .text
    .global syscall
syscall:
    ecall
    ret