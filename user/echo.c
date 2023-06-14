#include "system.h"
#include "ulib.h"

int main(int argc, char **argv, char **envp) {
    for (int i = 1; i < argc; ++i) {
        printf("%s ", argv[i]);
    }
    printf("\n");
    return 0;
}
