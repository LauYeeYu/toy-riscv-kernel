# System Call

Sytem calls (or syscalls) provides a method for user processes to ask for
services from the kernel.

The kernel provides a set of syscalls for user processes (listed in the
[types section](#types)). These syscalls are already wrapped in the
user library, so that user processes can call them easily without caring
about the convention.

## Types

|    Name     | ID |             Description             |
|:-----------:|:--:| ----------------------------------- |
|    fork     |  1 | Create a new process                |
|    exec     |  2 | Execute a program                   |
|    exit     |  3 | Exit current process                |
|    wait     |  4 | Wait for any one of child processes |
|  wait_pid   |  5 | Wait for a certain process          |
| send_signal |  6 | Kill a process                      |
|  put_char   |  7 | Put a character to screen           |
|  get_char   |  8 | Get a character from keyboard       |
|    yield    |  9 | Yield to other processes            |

## Convention

The input arguments are limited to 7 registers, i.e., `a0` to `a6`. `a7` is
reserved for the syscall id. The return value is stored in `a0`.

To boost the efficiency of copying from user space to kernel space, as well
as copying from kernel space to user space, any related syscalls will use
the `task->shared_memory` to send and receive data. In user space, the
`task->shared_memory` is a page under trap frame (two page below the
trampoline).

## Details

- [fork](#fork)
- [exec](#exec)
- [exit](#exit)
- [wait](#wait)
- [wait_pid](#wait_pid)
- [send_signal](#send_signal)
- [put_char](#put_char)
- [get_char](#get_char)

### fork

```c
pid_t fork();
```

Create a new process. The new process will be a copy of the current process,
except that the new process has a new process id, and the return value is
different in the two processes.

If the fork call is successful, the return value is 0 in the new process,
and the process id of the new process in the current process. If not, it
will return -1.

### exec

```c
int exec(const char *name, char *const argv[], char *const envp[]);
```

Execute a program. The program is specified by `name` and `argv`. The
program will be loaded into the current process, and the current process
will be replaced by the new program.

If the exec call is successful, it will not return. If not, it will return
-1.

The length of `name`, `argv` and `envp` must be less than a page (including
the '\0' at the end of each string).

Note: different from normal syscalls, the `exec` syscall uses the fourth and
fifth argument in the syscall ABI to kernel as the number of arguments in
`argv` and `envp` respectively.

### exit

```c
void exit(int status);
```

Exit current process. The exit status is specified by `status`.

### wait

```c
pid_t wait(int *status);
```
Suspend execution of the calling thread until one of its children terminates.
The exit status of the exited child process will be stored in `status` if
`status` is not NULL. If you don't care about the exit status, you can pass
NULL to `status`.

This function will return the process id of the exited child process, or
-1 if there are no child of this program.

### wait_pid

```c
pid_t wait_pid(pid_t pid, int *status);
```

Suspend execution of the calling
thread until a child specified by *pid* argument has changed state. (See the
table below for the meaning of *pid* argument.) The exit status of the exited
child process will be stored in `status` if `status` is not NULL. If you
don't care about the exit status, you can pass NULL to `status`.

This function will return the process id of the exited child process, or
-1 if there are no child of this program.

| value |                         meaning                         |
|:-----:| ------------------------------------------------------- |
|   -1  | any child process                                       |
|  > 0  | the child whose process ID is equal to the value of pid |

### send_signal

```c
int send_signal(pid_t pid, int sig);
```

Send a signal to a process. The process is specified by `pid`. The `pid`
must be positive and cannot be 1. The signal is specified by `sig`.

Return 0 if succeed; -1 if failed.

### put_char

```c
void put_char(int character);
```

Put a character on the terminal.

### get_char

```c
char get_char();
```

Get a character from the terminal.

### yield

```c
void yield();
```

Yield to other processes.
