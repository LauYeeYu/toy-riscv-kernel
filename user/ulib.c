#include "ulib.h"

#include <stdarg.h>

#include "system.h"

int print_char(char c) {
    put_char(c);
    return 1;
}

int print_string(const char *s) {
    int i = 0;
    while (s[i] != '\0') {
        put_char(s[i]);
        i++;
    }
    return i;
}

int print_unsigned_int(uint64 n) {
    if (n == 0) {
        put_char('0');
        return 1;
    }
    int length = n >= 10 ? print_unsigned_int(n / 10) : 0;
    put_char('0' + n % 10);
    return length + 1;
}

int print_int_dec(int64 n, bool sign) {
    int length = 0;
    if (sign) {
        if (n < 0) {
            put_char('-');
            n = -n;
            length++;
        }
    }
    length += print_unsigned_int(n);
    return length;
}

int print_int_hex_without_header(uint64 n) {
    if (n == 0) {
        put_char('0');
        return 1;
    }
    int length = n >= 16 ? print_int_hex_without_header(n / 16) : 0;
    int digit = n % 16;
    if (digit < 10) {
        put_char('0' + digit);
    } else {
        put_char('a' + digit - 10);
    }
    return length + 1;
}

int print_int_hex(uint64 n) {
    print_string("0x");
    int length = print_int_hex_without_header(n);
    return length + 2; // 2 for "0x"
}

int print_int_oct_without_header(uint64 n) {
    if (n == 0) {
        put_char('0');
        return 1;
    }
    int length = n >= 8 ? print_int_oct_without_header(n / 8) : 0;
    put_char('0' + n % 8);
    return length + 1;
}

int print_int_oct(uint64 n) {
    put_char('0');
    if (n == 0) return 1;
    int length = print_int_oct_without_header(n);
    return length + 1; // 1 for "0"
}

int print_int(int64 n, bool sign, int base) {
    if (base == 16) {
        return print_int_hex(n);
    } else if (base == 10) {
        return print_int_dec(n, sign);
    } else if (base == 8) {
        return print_int_oct(n);
    } else {
        return -1;
    }
}

int vprintf(const char *format, va_list args) {
    int length = 0;
    for (char c = *format; c != '\0'; c = *(++format)) {
        if (c == '%') {
            format++;
            if (*format == '\0') {
                return -1;
            }
            switch (*format) {
                case 'c': { // Character
                    char ch = va_arg(args, int);
                    print_char(ch);
                    length += 1;
                    break;
                }
                case 'd':
                case 'i': { // Integer
                    int64 n = va_arg(args, int);
                    length += print_int(n, true, 10);
                    break;
                }
                case 'o': { // Octal
                    uint64 n = va_arg(args, uint64);
                    length += print_int_oct(n);
                    break;
                }
                case 'p': { // Pointer
                    uint64 n = va_arg(args, uint64);
                    length += print_int_hex(n);
                    break;
                }
                case 's': { // String
                    char *s = va_arg(args, char *);
                    length += print_string(s);
                    break;
                }
                case 'u': { // Unsigned integer
                    uint64 n = va_arg(args, uint64);
                    length += print_int(n, false, 10);
                    break;
                }
                case 'x': { // Hexadecimal
                    uint64 n = va_arg(args, uint64);
                    length += print_int(n, false, 16);
                    break;
                }
                case '%':
                    print_char('%');
                    length += 1;
                    break;
                default:
                    return -1;
            }
        } else {
            print_char(c);
            length += 1;
        }
    }
    return length;
}

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int length = vprintf(format, args);
    va_end(args);
    return length;
}
