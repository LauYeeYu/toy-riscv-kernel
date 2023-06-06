#include "process.h"

#include "defs.h"
#include "mem_manage.h"
#include "memlayout.h"
#include "panic.h"
#include "print.h"
#include "riscv.h"
#include "riscv_defs.h"
#include "signal_defs.h"
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
    void *shared_memory = allocate(0);
    if (task->kernel_stack == NULL || task->pagetable == NULL ||
        user_stack == NULL || shared_memory == NULL ||
        task->trap_frame == NULL) {
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
    task->shared_memory = shared_memory;
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

int is_alive(struct task_struct *task) {
    return task != NULL && (task->state != ZOMBIE || task->state != DEAD);
}

int has_alive_child(struct task_struct *task) {
    struct single_linked_list_node *node = all_tasks->head;
    while (node != NULL) {
        struct task_struct *child = (struct task_struct *)(node->data);
        if (child->parent == task && is_alive(child)) return 1;
        node = node->next;
    }
    return 0;
}

struct task_struct *get_one_zombie_child(struct task_struct *task) {
    struct single_linked_list_node *node = all_tasks->head;
    while (node != NULL) {
        struct task_struct *child = (struct task_struct *)(node->data);
        if (child->parent == task && child->state == ZOMBIE) return child;
        node = node->next;
    }
    return NULL;
}

struct task_struct *find_task(pid_t pid) {
    struct single_linked_list_node *node = all_tasks->head;
    while (node != NULL) {
        struct task_struct *task = (struct task_struct *)(node->data);
        if (task->pid == pid) return task;
        node = node->next;
    }
    return NULL;
}

int is_ancestor(struct task_struct *ancestor, struct task_struct *task) {
    if (task == NULL || ancestor == NULL) return 0;
    struct task_struct *tmp = task->parent;
    while (tmp != NULL) {
        if (tmp == ancestor) return 1;
        tmp = tmp->parent;
    }
    return 0;
}

struct task_struct *child_with_pid(struct task_struct *task, pid_t pid) {
    struct task_struct *child = find_task(pid);
    if (child != NULL && child->parent == task && child->state != ZOMBIE) {
        return child;
    }
    return NULL;
}

uint64 fork_process(struct task_struct *task) {
    struct task_struct *new_task = kmalloc(sizeof(struct task_struct));
    if (new_task == NULL) return -1;
    new_task->kernel_stack = allocate(0); // 4KiB stack is enough
    new_task->pagetable = create_void_pagetable();
    new_task->trap_frame = allocate(0);
    void *new_shared_memory = allocate(0);
    if (new_task->kernel_stack == NULL || new_task->pagetable == NULL ||
        new_task->trap_frame == NULL || new_shared_memory == NULL) {
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
    new_task->shared_memory = new_shared_memory;
    push_tail(runnable_tasks, make_single_linked_list_node(new_task));
    push_tail(all_tasks, make_single_linked_list_node(new_task));
    return new_task->pid;
}

void exit_process(struct task_struct *task, int status) {
    if (task == NULL) {
        panic("exit_process: the process is NULL");
    }
    if (task->parent == NULL) {
        panic("exit_process: exit from init process");
    }
    free_user_memory(task);
    task->state = ZOMBIE;
    task->exit_status = status;
    task = NULL;
    for_each_node(all_tasks, reparent);
    if (task->parent->state == SLEEPING) {
        if (task->parent->channel == task->parent ||
            task->parent->channel == task) {
            task->parent->state = RUNNABLE;
            task->parent->trap_frame->a0 = task->pid;
            // save in a1 temporarily
            task->parent->trap_frame->a1 = task->exit_status;
            push_tail(runnable_tasks,
                      make_single_linked_list_node(task->parent));
        }
    }
    yield();
}

uint64 exec_process(struct task_struct *task, const char *name,
                  char *const argv[]) {
    // TODO
    return NULL;
}

void sleep(struct task_struct *task, void *channel) {
    task->state = SLEEPING;
    task->channel = channel;
    removeAt(runnable_tasks, task);
    yield();
}

/** Syscall part */

uint64 sys_fork(struct task_struct *task);
uint64 sys_exec(struct task_struct *task);
uint64 sys_exit(struct task_struct *task);
uint64 sys_wait(struct task_struct *task);
uint64 sys_wait_pid(struct task_struct *task);
uint64 sys_send_signal(struct task_struct *task);
uint64 sys_put_char(struct task_struct *task);
uint64 sys_get_char(struct task_struct *task);

#define SYSCALL_FORK        1
#define SYSCALL_EXEC        2
#define SYSCALL_EXIT        3
#define SYSCALL_WAIT        4
#define SYSCALL_WAIT_PID    5
#define SYSCALL_SEND_SIGNAL 6
#define SYSCALL_PUT_CHAR    7
#define SYSCALL_GET_CHAR    8

static uint64 (*syscalls[])(struct task_struct *) = {
    [SYSCALL_FORK]        = sys_fork,
    [SYSCALL_EXEC]        = sys_exec,
    [SYSCALL_EXIT]        = sys_exit,
    [SYSCALL_WAIT]        = sys_wait,
    [SYSCALL_WAIT_PID]    = sys_wait_pid,
    [SYSCALL_SEND_SIGNAL] = sys_send_signal,
    [SYSCALL_PUT_CHAR]    = sys_put_char,
    [SYSCALL_GET_CHAR]    = sys_get_char
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
    struct task_struct *zombie_child = get_one_zombie_child(task);
    uint64 status_ptr = task->trap_frame->a0;
    if (zombie_child != NULL) {
        if (status_ptr != 0) {
            int *status = task->shared_memory;
            *status = zombie_child->exit_status;
        }
        zombie_child->state = DEAD;
        return zombie_child->pid;
    }
    if (has_alive_child(task) == NULL) return -1;
    sleep(task, task);
    // running again
    if (status_ptr != 0) {
        int *status = task->shared_memory;
        *status = task->trap_frame->a1; // the status is saved in a1
    }
    return task->trap_frame->a0;
}

uint64 sys_wait_pid(struct task_struct *task) {
    pid_t pid = task->trap_frame->a0;
    void *channel = NULL;
    struct task_struct *zombie_child = NULL;
    if (pid == -1) {
        zombie_child = get_one_zombie_child(task);
        if (zombie_child == NULL) {
            if (has_alive_child(task) == NULL) return -1;
        }
        channel = task;
    } else {
        struct task_struct *child = child_with_pid(task, pid);
        if (child == NULL) return -1;
        if (child->state == ZOMBIE) {
            zombie_child = child;
        } else {
            channel = child;
        }
    }
    uint64 status_ptr = task->trap_frame->a1;
    if (zombie_child != NULL) {
        if (status_ptr != 0) {
            int *status = task->shared_memory;
            *status = zombie_child->exit_status;
        }
        zombie_child->state = DEAD;
        return zombie_child->pid;
    }
    sleep(task, channel);
    // running again
    if (status_ptr != 0) {
        int *status = task->shared_memory;
        *status = task->trap_frame->a1; // the status is saved in a1
    }
    return task->trap_frame->a0;
}

uint64 sys_send_signal(struct task_struct *task) {
    pid_t pid = task->trap_frame->a0;
    int signal = task->trap_frame->a1;
    struct task_struct *target = find_task(pid);
    if (is_ancestor(task, target) == 0 || is_alive(target)) return -1;
    switch (signal) {
        case NOTHING: // do nothing
            break;
        case SIGINT:
            exit_process(target, 2);
            break;
        case SIGKILL:
            exit_process(target, 9);
            break;
        default:
            break;
    }
    return signal;
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
