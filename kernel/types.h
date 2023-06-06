#ifndef TOY_RISCV_KERNEL_KERNEL_TYPES_H
#define TOY_RISCV_KERNEL_KERNEL_TYPES_H

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
typedef int64  pid_t;

#define NULL (0)

#endif //TOY_RISCV_KERNEL_KERNEL_TYPES_H
