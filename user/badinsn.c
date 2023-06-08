#include "system.h"

int main() {
    asm volatile(".byte 0x00, 0x00, 0x00");
    put_char('F');
    put_char('a');
    put_char('i');
    put_char('l');
    while (1)
        ;
}
