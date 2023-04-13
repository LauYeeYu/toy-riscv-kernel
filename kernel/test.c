#include "test.h"

#include "defs.h"
#include "print.h"

#ifdef TOY_RISCV_KERNEL_TEST_MEM_MANAGE
#include "mem_manage.h"
#endif // TOY_RISCV_KERNEL_TEST_MEM_MANAGE

void test() {
#ifdef TOY_RISCV_KERNEL_TEST_MEM_MANAGE
    print_string("Test memory management...\n");
    test_mem_manage();
#endif // TOY_RISCV_KERNEL_TEST_MEM_MANAGE
}
