#ifndef TOY_RISCV_KERNEL_KERNEL_UTILITY_H
#define TOY_RISCV_KERNEL_KERNEL_UTILITY_H

#include "types.h"

#define max(a, b) ({                             \
    typeof(a) __max1__ = (a);                    \
    typeof(b) __max2__ = (b);                    \
    (void)(&__max1__ == &__max2__);              \
    __max1__ > __max2__ ? __max1__ : __max2__;})

#define min(a, b) ({                             \
    typeof(a) __min1__ = (a);                    \
    typeof(b) __min2__ = (b);                    \
    (void)(&__min1__ == &__min2__);              \
    __min1__ < __min2__ ? __min1__ : __min2__;})

static inline size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

static inline void strcpy(char *dest, const char *src, size_t size) {
    for (size_t i = 0; i < size; i++) {
        dest[i] = src[i];
    }
    dest[size] = '\0';
}

static inline char *strtok(char *str, const char *delim) {
    static char *last = NULL;
    if (str != NULL) {
        last = str;
    }
    if (last == NULL) {
        return NULL;
    }
    char *ret = last;
    while (*last != '\0') {
        for (size_t i = 0; delim[i] != '\0'; i++) {
            if (*last == delim[i]) {
                *last = '\0';
                last++;
                return ret;
            }
        }
        last++;
    }
    last = NULL;
    return ret;
}

static inline int strcmp(const char *s1, const char *s2) {
    while (*s1 != 0 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (int) ((unsigned char) *s1 - (unsigned char) *s2);
}

static inline void memcpy(void *dest, const void *src, size_t size) {
    for (size_t i = 0; i < size; i++) {
        ((char *)dest)[i] = ((char *)src)[i];
    }
}

static inline void memset(void *dest, int c, size_t size) {
    for (size_t i = 0; i < size; i++) {
        ((char *)dest)[i] = c;
    }
}

#endif // TOY_RISCV_KERNEL_KERNEL_UTILITY_H
