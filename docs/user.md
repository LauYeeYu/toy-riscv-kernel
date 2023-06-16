# User Program

## Entry Point

The entry point of a user program is `main` function.

The structure of `main` function is as follows:
```c
int main(int argc, char *argv[], char **envp) {
    // ...
    return 0;
}
```

Note: Different from normal C programs, the `main` function in user programs
must have a return value.

## System Calls

You need to `#include <system.h>` to use system calls.

For information about system calls, see
[the document for system call](docs/system_call.md).

## Library Functions

We provide some library functions for you to use.

To use these functions, you need to `#include <ulib.h>`.

### max

`max` is actually a macro, which returns the maximum of two numbers.

### min

`min` is actually a macro, which returns the minimum of two numbers.

### memset

Set the memory to a certain value.

```c
void *memset(void *s, int c, size_t n);
```

### memcpy

Copy memory from one place to another.

```c
void *memcpy(void *dest, const void *src, size_t n);
```

### strlen

Get the length of a string.

```c
int strlen(const char *s);
```

### strcmp

Compare two strings.

```c
int strcmp(const char *s1, const char *s2);
```

### strcpy

Copy a string.

```c
char *strcpy(char *dest, const char *src, size_t size);
```

### string_includes

Check if a string includes a character.

```c
static inline bool string_includes(const char *s, char c);
```

### strtok

Split a string into tokens.

```c
char *strtok(char *str, const char *delim);
```

### read_until

Read a string from stdin until a specified character. If we meet the specified
character, the string will include that character.

Return 0 if we meet the limit; return 1 if we meet the specified character.

```c
int read_until(char *buffer, int max_length, char end);
```

### print_char

Print a character to stdout.

```c
void print_char(char ch);
```

### print_string

Print a string to stdout.

```c
void print_string(const char *s);
```

### print_int

Print an integer to stdout.

```c
int print_int(int64 n, bool sign, int base);
```

### printf

Print a formatted string to stdout.

```c
int printf(const char *format, ...);
```
