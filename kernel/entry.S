# qemu -kernel loads the kernel at 0x80000000
# and causes each hart (i.e. CPU) to jump there.
# kernel.ld causes the following code to
# be placed at 0x80000000.
    .section .text
    .global _entry
_entry:
    # set up a stack for C.
    # stack is declared below.
    la sp, stack_top
    # jump to start() in start.c to continue the boot process
    call start
spin:
    j spin

# get the kernel end address
    .section .text
    .global get_kernel_end
get_kernel_end:
    la a0, kernel_end
    ret

    .section .bss.stack
    .global stack_bottom
stack_bottom:
    .space 4096 * 16 # 64KiB
    .global stack_top
stack_top:

    .section .kernel_end
    .global kernel_end
kernel_end:
