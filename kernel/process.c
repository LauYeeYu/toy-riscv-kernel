#include "process.h"

#include "defs.h"
#include "mem_manage.h"
#include "memlayout.h"
#include "panic.h"
#include "print.h"
#include "riscv.h"
#include "riscv_defs.h"
#include "single_linked_list.h"
#include "switch.h"
#include "syscall.h"
#include "trap.h"
#include "uart.h"
#include "virtual_memory.h"
#include "utility.h"

extern char trampoline[]; // from kernel.ld
extern pagetable_t kernel_pagetable;

pid_t next_pid = 1;

struct task_struct *new_task(
    const char *name,
    struct task_struct *parent,
    void *src_memory,
    size_t size
) {
    int map_result = 0;
    struct task_struct *task = kmalloc(sizeof(struct task_struct));
    if (task == NULL) return NULL;
    task->kernel_stack = allocate(0); // 4KiB stack is enough
    task->pagetable = create_void_pagetable();
    void *user_stack = allocate(0);
    task->trap_frame = allocate(0);
    if (task->kernel_stack == NULL || task->pagetable == NULL ||
        user_stack == NULL || task->trap_frame == NULL) {
        deallocate(task->kernel_stack, 0);
        deallocate(user_stack, 0);
        deallocate(task->pagetable, 0);
        kfree(task);
        return NULL;
    }
    task->memory_size = init_virtual_memory_for_user(
        task->pagetable,
        src_memory,
        size,
        PTE_R | PTE_X | PTE_U
    );
    if (task->memory_size == 0) {
        deallocate(task->kernel_stack, 0);
        deallocate(task->pagetable, 0);
        kfree(task);
        return NULL;
    }
    task->memory_size += PGSIZE;
    task->trap_frame->kernel_satp = (uint64)kernel_pagetable;
    task->trap_frame->epc = 0;
    task->trap_frame->sp = task->memory_size;
    map_result |= map_page(
        task->pagetable,
        task->memory_size - PGSIZE,
        (uint64)user_stack,
        PTE_R | PTE_W | PTE_U
    );
    map_result |= map_page(
        task->pagetable,
        (uint64)TRAPFRAME,
        (uint64)task->trap_frame,
        PTE_R | PTE_W
    );
    map_result |= map_page(
        task->pagetable,
        (uint64)TRAMPOLINE,
        (uint64)trampoline,
        PTE_R | PTE_X
    );
    if (map_result != 0) {
        deallocate(task->kernel_stack, 0);
        deallocate(user_stack, 0);
        free_memory(task->pagetable, 0, task->memory_size);
        deallocate(task->pagetable, 0);
        kfree(task);
        return NULL;
    }
    task->state = RUNNABLE;
    task->pid = next_pid++;
    task->parent = parent;
    task->context.sp = (uint64)task->kernel_stack + PGSIZE;
    task->context.ra = (uint64)user_trap_return;
    strcpy(task->name, name, min(31UL, strlen(name)));
    return task;
}

void free_user_memory(struct task_struct *task) {
    deallocate(task->kernel_stack, 0);
    deallocate(task->trap_frame, 0);
    free_memory(task->pagetable, 0, task->memory_size);
    free_pagetable(task->pagetable);
}

/** Scheduler part */

struct single_linked_list_node *running_task = NULL;
struct task_struct *init = NULL;
struct single_linked_list *runnable_tasks = NULL;
struct single_linked_list *all_tasks = NULL;
struct context now_context;

struct task_struct *current_task() {
    if (runnable_tasks == NULL) return NULL;
    return (struct task_struct *)(running_task->data);
}

void init_scheduler() {
    runnable_tasks = create_single_linked_list();
    all_tasks = create_single_linked_list();
#ifndef TOY_RISCV_KERNEL_TEST_SCHEDULER
    //TODO: add /init here
#endif // NOT TOY_RISCV_KERNEL_TEST_SCHEDULER
}

void scheduler() {
    for (;;) {
        interrupt_off();
        if (running_task != NULL) {
            panic("scheduler: trying to run a task while another task is running");
        }
        if (runnable_tasks->size > 0) {
            struct single_linked_list_node *task_node = head_node(runnable_tasks);
            running_task = task_node;
            struct task_struct *task = task_node->data;
            pop_head(runnable_tasks);
            switch_context(&now_context, &(task->context));
        }
        interrupt_on();
    }

}

void yield() {
    interrupt_off();
    struct context *old_context = &now_context;
    struct task_struct *task = current_task();
    if (task != NULL) {
        task->state = RUNNABLE;
        push_tail(runnable_tasks, running_task);
        old_context = &(task->context);
    }

    running_task = head_node(runnable_tasks);
    struct task_struct *new_task = current_task();
    pop_head(runnable_tasks);
    new_task->state = RUNNING;
    interrupt_on();
    switch_context(old_context, &(new_task->context));
}

#ifdef TOY_RISCV_KERNEL_TEST_SCHEDULER
uint32 program1[] = {
    0x10000537, // lui a0,0x10000
    0x0310059b, // addiw a1,zero,0x31
    0x00b50023, // sb a1,0(a0)
    0xbfd5 // j 0
};

uint32 program2[] = {
    0x10000537, // lui a0,0x10000
    0x0320059b, // addiw a1,zero,0x32
    0x00b50023, // sb a1,0(a0)
    0xbfd5 // j 0
};

void test_scheduler() {
    struct task_struct *task1 = new_task("task1", NULL, program1, sizeof(program1));
    struct task_struct *task2 = new_task("task2", NULL, program2, sizeof(program2));
    map_page(task1->pagetable, UART0, UART0, PTE_R | PTE_W | PTE_X | PTE_U);
    map_page(task2->pagetable, UART0, UART0, PTE_R | PTE_W | PTE_X | PTE_U);
    push_tail(runnable_tasks, make_single_linked_list_node(task1));
    push_tail(all_tasks, make_single_linked_list_node(task1));
    push_tail(runnable_tasks, make_single_linked_list_node(task2));
    push_tail(all_tasks, make_single_linked_list_node(task2));
    scheduler();
}
#endif // TOY_RISCV_KERNEL_TEST_SCHEDULER

void reparent(void *data) {
    struct task_struct *task = (struct task_struct *)data;
    if (task->parent != NULL && task->parent->state == ZOMBIE) {
        task->parent = init;
    }
}

uint64 fork_process(struct task_struct *task) {
    struct task_struct *new_task = kmalloc(sizeof(struct task_struct));
    if (new_task == NULL) return -1;
    new_task->kernel_stack = allocate(0); // 4KiB stack is enough
    new_task->pagetable = create_void_pagetable();
    new_task->trap_frame = allocate(0);
    if (new_task->kernel_stack == NULL || new_task->pagetable == NULL || new_task->trap_frame == NULL) {
        deallocate(task->kernel_stack, 0);
        kfree(new_task);
        return -1;
    }
    new_task->memory_size = task->memory_size;
    new_task->trap_frame = allocate(0);
    int map_result = 0;
    map_result |= copy_memory_with_pagetable(
        task->pagetable,
        new_task->pagetable,
        0,
        task->memory_size
    );
    *(new_task->trap_frame) = *(task->trap_frame);
    new_task->trap_frame->a0 = 0; // fork() returns 0 in the child process
    map_result |= map_page(
        new_task->pagetable,
        (uint64)TRAPFRAME,
        (uint64)new_task->trap_frame,
        PTE_R | PTE_W
    );
    map_result |= map_page(
        new_task->pagetable,
        (uint64)TRAMPOLINE,
        (uint64)trampoline,
        PTE_R | PTE_X
    );
    if (map_result != 0) {
        deallocate(new_task->kernel_stack, 0);
        free_memory(new_task->pagetable, 0, new_task->memory_size);
        free_pagetable(new_task->pagetable);
        kfree(new_task);
        return -1;
    }
    new_task->state = RUNNABLE;
    new_task->pid = next_pid++;
    new_task->parent = task;
    new_task->context.sp = (uint64)new_task->kernel_stack + PGSIZE;
    new_task->context.ra = (uint64)user_trap_return;
    strcpy(new_task->name, task->name, min(31UL, strlen(task->name)));
    push_tail(runnable_tasks, make_single_linked_list_node(new_task));
    push_tail(all_tasks, make_single_linked_list_node(new_task));
    return new_task->pid;
}

void exit_process(struct task_struct *task, int status) {
    if (task == NULL) {
        panic("exit_process: the process is NULL");
    }
    free_user_memory(task);
    task->state = ZOMBIE;
    task->exit_status = status;
    task = NULL;
    for_each_node(all_tasks, reparent);
    yield();
}

uint64 exec_process(struct task_struct *task, const char *name,
                  char *const argv[]) {
    // TODO
    return NULL;
}

/** Syscall part */

uint64 sys_fork(struct task_struct *task);
uint64 sys_exec(struct task_struct *task);
uint64 sys_exit(struct task_struct *task);
uint64 sys_wait(struct task_struct *task);
uint64 sys_wait_pid(struct task_struct *task);
uint64 sys_kill(struct task_struct *task);
uint64 sys_put_char(struct task_struct *task);
uint64 sys_get_char(struct task_struct *task);

#define SYSCALL_FORK     1
#define SYSCALL_EXEC     2
#define SYSCALL_EXIT     3
#define SYSCALL_WAIT     4
#define SYSCALL_WAIT_PID 5
#define SYSCALL_KILL     6
#define SYSCALL_PUT_CHAR 7
#define SYSCALL_GET_CHAR 8

static uint64 (*syscalls[])(struct task_struct *) = {
    [SYSCALL_FORK]     = sys_fork,
    [SYSCALL_EXEC]     = sys_exec,
    [SYSCALL_EXIT]     = sys_exit,
    [SYSCALL_WAIT]     = sys_wait,
    [SYSCALL_WAIT_PID] = sys_wait_pid,
    [SYSCALL_KILL]     = sys_kill,
    [SYSCALL_PUT_CHAR] = sys_put_char,
    [SYSCALL_GET_CHAR] = sys_get_char
};

void syscall() {
    struct task_struct *current = current_task();
    if (current == NULL) panic("syscall: no task running!");
    int id = current->trap_frame->a7;
    if (id > 0 && id < sizeof(syscalls) / sizeof(syscalls[0])) {
        // The return value should be returned to user
        current->trap_frame->a0 = syscalls[id](current);
    } else {
        print_string("syscall: unknown syscall id: ");
        print_int(id, 10);
        print_string(".\n");
        current->trap_frame->a0 = -1;
    }
}

uint64 sys_fork(struct task_struct *task) {
    return fork_process(task);
}

uint64 sys_exec(struct task_struct *task) {
    // TODO
    return NULL;
}

uint64 sys_exit(struct task_struct *task) {
    exit_process(task, task->trap_frame->a0);
    return 0;
}

uint64 sys_wait(struct task_struct *task) {
    // TODO
    return NULL;
}

uint64 sys_wait_pid(struct task_struct *task) {
    // TODO
    return NULL;
}

uint64 sys_kill(struct task_struct *task) {
    // TODO
    return NULL;
}

uint64 sys_put_char(struct task_struct *task) {
    char c = (char)task->trap_frame->a0;
    print_char(c);
    return 0;
}

uint64 sys_get_char(struct task_struct *task) {
    int c;
    while ((c = uart_getc()) == -1 && runnable_tasks->size > 0) yield();
    while ((c = uart_getc()) == -1) asm volatile("wfi");
    return c;
}
