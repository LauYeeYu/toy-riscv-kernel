#ifndef TOY_RISCV_KERNEL_USER_SYSTEM_H
#define TOY_RISCV_KERNEL_USER_SYSTEM_H

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

typedef signed char  int8;
typedef signed short int16;
typedef signed int   int32;
typedef signed long  int64;

typedef uint64 pte_t;
typedef uint64 size_t;
typedef uint64 reg_t;
typedef uint64 pid_t;

#define NULL (0)

pid_t fork();

int exec(const char *name, char *const argv[], char *const envp[]);

void exit(int status) __attribute__((noreturn));

int wait(int *status);

pid_t wait_pid(pid_t pid, int *status);

int send_signal(pid_t pid, int sig);

void yield();

int power_off();

void put_char(int character);

char get_char();

#endif // TOY_RISCV_KERNEL_USER_SYSTEM_H