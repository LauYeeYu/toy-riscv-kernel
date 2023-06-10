#include "system.h"

#define SYSCALL_FORK        1
#define SYSCALL_EXEC        2
#define SYSCALL_EXIT        3
#define SYSCALL_WAIT        4
#define SYSCALL_WAIT_PID    5
#define SYSCALL_SEND_SIGNAL 6
#define SYSCALL_YIELD       7
#define SYSCALL_POWER_OFF   8
#define SYSCALL_PUT_CHAR    9
#define SYSCALL_GET_CHAR    10

#define PGSIZE 4096

// maximum allowed user program size
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

// map the trampoline page to the highest address,
// in both user and kernel space.
#define TRAMPOLINE (MAXVA - PGSIZE)
#define SHARED_MEMORY (TRAMPOLINE - PGSIZE * 2)

uint64 syscall(uint64 arg1, uint64 arg2, uint64 arg3, uint64 arg4,
               uint64 arg5, uint64 arg6, uint64 arg7, uint64 id);

pid_t fork() {
    return syscall(0, 0, 0, 0, 0, 0, 0, SYSCALL_FORK);
}

int exec(const char *name, char *const argv[], char *const envp[]) {
    int state = 0; // 0: name, 1: argv, 2: envp
    int count1 = 0, count2 = 0;
    int argv_size = 0;
    int envp_size = 0;
    char *shared_memory = (char *)SHARED_MEMORY;
    for (int i = 0; i < 4096; ++i) {
        switch (state) {
            case 0: { // name
                shared_memory[i] = name[i];
                if (name[i] == '\0') state = 1; // move to argv
                break;
            }
            case 1: { // argv
                if (argv[count1] == NULL) {
                    state = 2;
                    argv_size = count1;

                    // reset count1 and count2 and decrement i
                    count1 = 0;
                    count2 = 0;
                    --i; // This will not take any space
                } else {
                    shared_memory[i] = argv[count1][count2];
                    if (argv[count1][count2] == '\0') {
                        count1++;
                        count2 = 0;
                    } else {
                        count2++;
                    }
                }
                break;
            }
            case 2: {
                if (envp[count1] == NULL) {
                    shared_memory[i] = '\0';
                    envp_size = count1;
                } else {
                    shared_memory[i] = envp[count1][count2];
                    if (envp[count1][count2] == '\0') {
                        count1++;
                        count2 = 0;
                    } else {
                        count2++;
                    }
                }
                break;
            }
        }
    }
    shared_memory[PGSIZE - 1] = '\0';
    return syscall((uint64)name, (uint64)argv, (uint64)envp,
                   argv_size, envp_size, 0, 0, SYSCALL_EXEC);
}

void exit(int status) {
    syscall(status, 0, 0, 0, 0, 0, 0, SYSCALL_EXIT);
    for (;;) {} // actually not reachable, but to avoid compiler warning
}

int wait(int *status) {
    int return_val = syscall((uint64)status, 0, 0, 0, 0, 0, 0, SYSCALL_WAIT);
    if (return_val != NULL) {
        int *shared_memory = (int *)SHARED_MEMORY;
        *status = *shared_memory;
    }
    return return_val;
}

pid_t wait_pid(pid_t pid, int *status) {
    int return_val = syscall((uint64)pid, (uint64)status, 0, 0, 0, 0, 0,
                             SYSCALL_WAIT_PID);
    if (return_val != NULL) {
        int *shared_memory = (int *)SHARED_MEMORY;
        *status = *shared_memory;
    }
    return return_val;
}

int send_signal(pid_t pid, int sig) {
    return syscall((uint64) pid, (uint64)sig, 0, 0, 0, 0, 0,
                   SYSCALL_SEND_SIGNAL);
}

void yield() {
    syscall(0, 0, 0, 0, 0, 0, 0, SYSCALL_YIELD);
}

int power_off() {
    return syscall(0, 0, 0, 0, 0, 0, 0, SYSCALL_POWER_OFF);
}

void put_char(int character) {
    syscall((uint64)character, 0, 0, 0, 0, 0, 0, SYSCALL_PUT_CHAR);
}

char get_char() {
    return (char)syscall(0, 0, 0, 0, 0, 0, 0, SYSCALL_GET_CHAR);
}

int main(int argc, char *const argv[], char *const envp[]);

__attribute__((noreturn)) void _start(int argc, char *const argv[], char *const envp[]) {
    exit(main(argc, argv, envp));
}
