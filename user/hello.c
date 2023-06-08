#include "system.h"

int main() {
    char *hello = "Hello, world!\n";
    for (int i = 0; i < 14; i++) {
        put_char(hello[i]);
    }
    exit(0);
}
