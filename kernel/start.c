#include "print.h"
#include "uart.h"

void start() {
    uart_init();
    print_string("Hello, world!");
}
