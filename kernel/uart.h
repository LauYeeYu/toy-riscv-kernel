#ifndef TOY_RISCV_KERNEL_KERNEL_UART_H
#define TOY_RISCV_KERNEL_KERNEL_UART_H

void uart_init();
void uart_putc(int c);
void uart_putc_sync(int c);
int uart_getc();

// handle a uart interrupt, raised because input has arrived, or the uart is
// ready for more output, or both.
void uart_intr();

#endif //TOY_RISCV_KERNEL_KERNEL_UART_H
