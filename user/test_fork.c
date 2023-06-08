#include "system.h"

int main() {
    if (fork()) {
        for (;;) put_char('1');
    } else {
        for (;;) put_char('2');
    }
}
