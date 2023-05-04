#include "print.h"

#include "spinlock.h"
#include "uart.h"

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

void print_int_dec(uint64 x) {
    if (x == 0) {
        uart_putc_sync('0');
    } else {
        if (x >= 10) {
            print_int_dec(x / 10);
        }
        uart_putc_sync('0' + x % 10);
    }
}

void print_int_hex_without_header(uint64 x) {
    if (x == 0) {
        uart_putc_sync('0');
    } else {
        if (x >= 16) {
            print_int_hex_without_header(x / 16);
        }
        int digit = x % 16;
        if (digit < 10) {
            uart_putc_sync('0' + digit);
        } else {
            uart_putc_sync('a' + digit - 10);
        }
    }
}

void print_int(uint64 x, int base) {
    acquire(&lock);
    switch (base) {
        case 10:
            print_int_dec(x);
            break;
        case 16:
            uart_putc_sync('0');
            uart_putc_sync('x');
            print_int_hex_without_header(x);
            break;
        default:
            print_string("print_int: unsupported base: ");
            print_int(base, 10);
            break;
    }
    release(&lock);
}
