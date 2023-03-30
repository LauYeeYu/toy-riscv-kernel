#include "print.h"
#include "uart.h"

// entry.S jumps here in machine mode.
void start() {
    uart_init();
    print_string("Entering kernel...\n");
}
