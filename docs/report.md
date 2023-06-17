# Feature Report

## Bootloader

Bootloader 分为两部分，首先是一段汇编代码，用于初始化栈和进入 C 语言的入口点。然后是一段 C 语言代码，用于初始化内存和进入内核。

汇编代码位于 `kernel/entry.S`，C 语言代码位于 `kernel/start.c`。

Bootloader 会静态地跳转到 `kernel/main`（同时切换到 S mode），然后进入内核。

<details>
<summary>具体代码</summary>

`user/entry.S`:

```asm
_entry:
    # set up a stack for C.
    # stack is declared below.
    la sp, stack_top
    # jump to start() in start.c to continue the boot process
    call start
```

`user/start.c`:

```c
void start() {
    uart_init();
    print_string("Entering kernel...\n");

    // Set up an identically-mapping page table. (using huge pages)
    print_string("Establishing page table... ");
    establish_page_table();
    print_string("Done.\n");

    print_string("Setting mstatus... ");
    unsigned long x = read_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    write_mstatus(x);
    print_string("Done.\n");

    print_string("Setting things for interruption... ");
    write_medeleg(0xffff);
    write_mideleg(0xffff);
    write_sie(read_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
    print_string("Done.\n");

    // configure Physical Memory Protection to give supervisor mode
    // access to all of physical memory.
    print_string("Setting PMP... ");
    write_pmpaddr0(0x3fffffffffffffull);
    write_pmpcfg0(0xf);
    print_string("Done.\n");

    print_string("Enabling paging... ");
    // Set the page table.
    write_satp(((uint64)pagetable >> 12) | SATP_SV39);
    // Enable paging.
    write_sstatus((read_sstatus() | SSTATUS_SPP) & ~SSTATUS_SIE);
    print_string("Done.\n");

    write_mepc((uint64)main);

    print_string("Setting timer... ");
    init_timer();
    print_string("Done.\n");

    // enter supervisor mode, and jump to main().
    asm volatile("mret");
}
```
</details>


## 内存初始化

kernel 之外的剩余空间将会被用于分配物理页。kernel 采用 buddy 伙伴算法管理内存。

初始化过程中，kernel 结尾之后的所有页都会被分配给 buddy 算法，然后按照 2 的幂次分配给不同的空闲链表。初始化的实现位于 `kernel/mem_manage.c` 中的 `init_mem_manage`。

<details>
<summary>具体代码</summary>

```c
void init_mem_manage() {
    size_t start_addr = get_kernel_end();
    size_t kernel_size = start_addr - KERNEL_START;
    size_t buddy_size = KERNEL_MEM_SIZE / 2;
    int index = BUDDY_MAX_ORDER - 1;
    buddy_pool.space[BUDDY_MAX_ORDER].next = NULL;
    while (buddy_size >= kernel_size && index >= 0) {
        buddy_pool.space[index].next = (node *)(KERNEL_START + buddy_size);
        // Set the next pointer to NULL
        buddy_pool.space[index].next->next = NULL;
        buddy_size >>= 1;
        index--;
    }

    void *tmp = (void *)(KERNEL_START + buddy_size);
    while (index >= 0) {
        if ((size_t)tmp >= start_addr) { // available
            buddy_pool.space[index].next = tmp;
            buddy_pool.space[index].next->next = NULL;
            index--;
            tmp = (void *)((size_t)tmp - (PAGE_SIZE << index));
        } else {
            buddy_pool.space[index].next = NULL;
            index--;
            tmp = (void *)((size_t)tmp + (PAGE_SIZE << index));
        }
    }
#ifdef PRINT_BUDDY_DETAIL
    print_string("\n");
    print_buddy_pool();
#endif // PRINT_BUDDY_DETAIL
}
```
</details>

物理页的分配和释放位于 `kernel/mem_manage.c` 中的 `allocate` 和 `deallocate` 函数，每次只能获取 2 的幂次的页。

## 内存管理

*物理内存管理见上一节*

虚拟内存管理位于 `kernel/virtual_memory.c` 中，采用的是三级页表，支持页表创建、查找、分配、释放。可以使用 `create_void_pagetable` 创建一个空的页表，使用 `map_page` 将一个虚拟页映射到一个物理页，使用 `unmap_page` 将一个虚拟页取消映射。

`kernel/virtual_memory.c` 提供了多个函数用于方便地进行内存操作，具体参见 `kernel/virtual_memory.h` 中的声明。

## IO

IO 位于 `kernel/uart.c` 中，提供输入输出字符功能。

## 进程加载

进程加载位于 `kernel/elf.c` 中，提供了 `load_elf` 函数用于加载一个 ELF 文件。

`load_elf` 函数会对每个段分配空间，然后将段内容复制到对应的空间中。

## 调度

上下文切换只会在内核态发生（因为用户态会先 trap 到内核态，然后切换上文），位于 `kernel/switch.S` 中。

调度策略位于 `kernel/process.c` 中，采用的是简单的轮转调度。具体的实现参见 `kernel/process.c` 中的 `yield` 函数。

<details>
<summary>yield 实现</summary>

以下代码中 stack_to_remove 和 stack_to_remove_next 是用于释放栈的，由于一个任务在 exit 之后不能立刻释放栈空间（因为现在还在使用这个栈），必须在 yield 之后执行，因此放了两级缓冲，保证会在第一次 yield 之后才可能发生内核栈的回收。

```c
void yield() {
    interrupt_off();
#ifdef TOY_RISCV_KERNEL_PRINT_TASK
    print_string("current task: ");
    print_task_meta(current_task());
    print_all_task_meta();
#endif
    deallocate(stack_to_remove, 0);
    stack_to_remove = stack_to_remove_next;
    stack_to_remove_next = NULL;
    struct context *old_context = &now_context;
    struct task_struct *task = current_task();
    if (runnable_tasks->size == 0) return;
    if (task != NULL && task->state == RUNNING) {
        task->state = RUNNABLE;
        push_tail(runnable_tasks, running_task);
        old_context = &(task->context);
    }

    running_task = next_task_to_run();
    if (running_task == NULL) panic("yield: no task to run");
    struct task_struct *new_task = current_task();
    pop_head_without_free(runnable_tasks);
    new_task->state = RUNNING;
    interrupt_on();
    switch_context(old_context, &(new_task->context));
}
```
</details>

## 中断

中断位于 `kernel/trap.c` 中，提供了下列函数：

- `user_trap`: 用户态中断处理函数
- `kernel_trap`: 内核态中断处理函数
- `user_trap_return`: 用户态中断返回函数

### Syscalls

在系统调用时，syscall id 将会储存在 `a7` 中，参数将会储存在 `a0` 到 `a6` 中，返回值将会储存在 `a0` 中。

对于需要读写内存的系统调用，会利用在任务创建时建立的共享内存来传输，减少切换页表次数。

具体参见 [`docs/syscall.md`](syscall.md)。

实现的系统调用：

|    Name     | ID |             Description             |
|:-----------:|:--:| ----------------------------------- |
|    fork     |  1 | Create a new process                |
|    exec     |  2 | Execute a program                   |
|    exit     |  3 | Exit current process                |
|    wait     |  4 | Wait for any one of child processes |
|  wait_pid   |  5 | Wait for a certain process          |
| send_signal |  6 | Kill a process                      |
|    yield    |  7 | Yield to other processes            |
|  power_off  |  8 | Power off the machine               |
|  put_char   |  9 | Put a character to screen           |
|  get_char   | 10 | Get a character from keyboard       |


## 用户态

kernel 完全支持用户态，用户态的代码位于 `user` 目录下。

可以通过以下测试来验证用户态的正确性：

```bash
$ make qemu init=badcall
```

相关输出：

panic 是因为 init 退出会导致 kernel panic，但是这不影响用户态的正确性。

```
Illegal instruction: 0x30200073, pid 1
panic: exit_process: exit from init process
```

### 简单版本的用户态程序

- sh (见 `user/sh.c`)
- echo (见 `user/echo.c`)

<details>
<summary>运行结果</summary>

```
$ make qemu init=sh
...
Welcome to sh!
# e
execve failed!
-1 # echo a
a 
# echo hello world
hello world 
# sh
Welcome to sh!
# echo sh
sh 
# exit
# poweroff
```

## 关机

关机可通过 syscall 完成，具体实现为往 `VIRT_TEST` 地址写入 `0x5555`。

可以在 sh 中输入 `poweroff` 来关机。

<details>
<summary>关机实现</summary>

```c
uint64 sys_power_off(struct task_struct *task) {
    if (task->pid != 1) return -1;
    *(uint32 *)VIRT_TEST = 0x5555;
    return 0;
}
```
</details>
