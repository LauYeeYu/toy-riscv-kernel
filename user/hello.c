#include "system.h"

int main() {
    if (fork()) {
        while (1) put_char('1');
    }
    while (1) put_char('2');
    // char *hello = "Hello, world!\n";
    // for (int i = 0; i < 14; i++) {
    //     put_char(hello[i]);
    // }
    // exit(0);
}
