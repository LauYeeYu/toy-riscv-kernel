/**
 * To test whether the stack enlargement is correct or not.
 * Note: if you want to run this program, you should remove the
 * -Werror flag in the Makefile.
 */

#include "system.h"

int foo(int a) {
    put_char('a');
    return foo(a);
}
int main() {
    foo(0);
    return 0;
}
