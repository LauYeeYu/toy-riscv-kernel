#ifndef TOY_RISCV_KERNEL_USER_ULIB_H
#define TOY_RISCV_KERNEL_USER_ULIB_H

#include "system.h"

static inline void *memset(void *dst, int c, size_t n) {
    char *pos = (char *) dst;
    while (n--) {
        *pos++ = c;
    }
    return dst;
}

static inline void *memcpy(void *dst, const void *src, size_t n) {
    char *pos_dst = (char *) dst;
    const char *pos_src = (const char *) src;
    while (n--) {
        *pos_dst++ = *pos_src++;
    }
    return dst;
}

static inline int strlen(const char *s) {
    int n = 0;
    while (*s++) {
        n++;
    }
    return n;
}

static inline int strcmp(const char *s1, const char *s2) {
while (*s1 != 0 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (int) ((unsigned char) *s1 - (unsigned char) *s2);
}

/**
 * Read a string until the end character or the buffer is full.
 * @param buffer The buffer to store the string.
 * @param max_length The maximum length of the string.
 * @param end The end character.
 * @return 0 if success, -1 if the buffer is full.
 */
static inline int read_until(char *buffer, int max_length, char end) {
    int i = 0;
    while (i + 1 < max_length) {
        char c = get_char();
        buffer[i++] = c;
        if (c == end || c == '\0') {
            buffer[i] = '\0';
            return 0;
        }
    }
    buffer[i] = '\0';
    return -1;
}

int print_char(char c);
int print_string(const char *s);
int print_int(int64 n, bool sign, int base);
int printf(const char *format, ...);


#endif // TOY_RISCV_KERNEL_USER_ULIB_H
