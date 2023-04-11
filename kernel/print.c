#include "print.h"

#include "spinlock.h"
#include "uart.h"

volatile int panicked = 0;

// lock to avoid interleaving concurrent printf's.
static spinlock lock;

void print_string(const char *str) {
    acquire(&lock);
    while (*str != '\0') {
        uart_putc_sync(*str);
        str++;
    }
    release(&lock);
}

void print_int(uint64 x) {
    acquire(&lock);
    if (x == 0) {
        uart_putc_sync('0');
    } else {
        char buf[16];
        int i = 0;
        while (x > 0) {
            buf[i++] = '0' + x % 10;
            x /= 10;
        }
        while (i > 0) {
            uart_putc_sync(buf[--i]);
        }
    }
    release(&lock);
}
