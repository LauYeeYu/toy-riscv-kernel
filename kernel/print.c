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
