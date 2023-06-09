/*
 * Test the yield() system call.
 * It should print '1', and fail if it prints '2'.
 * Then it will spin forever.
 */

#include "system.h"

int main() {
    int status = 0x32;
    if (fork()) {
        yield();
        wait(&status);
        put_char(status);
        for (;;) {}
    } else {
        exit(0x31);
    }
}
