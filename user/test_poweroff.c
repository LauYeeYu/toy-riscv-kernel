#include "system.h"

int main() {
    if (fork()){
        yield();
        power_off();
        put_char('F');
        put_char('A');
        put_char('I');
        put_char('L');
    } else {
        if (power_off() == -1) {
            put_char('a');
        }
        exit(0);
    }
    return 0;
}
