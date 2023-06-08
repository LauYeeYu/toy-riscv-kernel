#include "system.h"


int main() {
    asm volatile("mret");
    put_char('F');
    put_char('a');
    put_char('i');
    put_char('l');
    while (1)
        ;
}
