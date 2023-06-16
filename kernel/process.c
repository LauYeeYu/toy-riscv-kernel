#include "process.h"

#include "defs.h"
#include "elf.h"
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
#include "types.h"
#include "uart.h"
#include "virtual_memory.h"
#include "utility.h"

extern char trampoline[]; // from kernel.ld
extern pagetable_t kernel_pagetable;

pid_t next_pid = 1;

void *stack_to_remove = NULL;
void *stack_to_remove_next = NULL;

#ifdef TOY_RISCV_KERNEL_PRINT_TASK
void print_task_meta(struct task_struct *task) {
    print_string("task: ");
    print_string(task->name);
    print_string(", pid: ");
    print_int(task->pid, 10);
    print_string(", parent: ");
    if (task->parent == NULL) {
        print_string("NULL");
    } else {
        print_int(task->parent->pid, 10);
    }
    switch (task->state) {
        case RUNNING:
            print_string(", state: RUNNING\n");
            break;
        case ZOMBIE:
            print_string(", state: ZOMBIE\n");
            break;
        case SLEEPING:
            print_string(", state: SLEEPING\n");
            break;
        case RUNNABLE:
            print_string(", state: RUNNABLE\n");
            break;
        case DEAD:
            print_string(", state: DEAD\n");
            break;
    }
}
#endif

void *allocate_for_user(size_t power) {
    void *addr = allocate(power);
    if (addr == NULL) return NULL;
    memset(addr, 0, PGSIZE << power);
    return addr;
}

struct task_struct *new_task(const char *name, struct task_struct *parent) {
    int map_result = 0;
    struct task_struct *task = kmalloc(sizeof(struct task_struct));
    if (task == NULL) return NULL;
    task->kernel_stack = allocate(0); // 4KiB stack is enough
    task->stack_permission = PTE_U | PTE_R | PTE_W;
    init_single_linked_list(&(task->mem_sections));
    task->pagetable = create_void_pagetable();
    task->trap_frame = allocate_for_user(0);
    void *shared_memory = allocate_for_user(0);
    memset(&(task->context), 0, sizeof(struct context));
    if (task->kernel_stack == NULL || task->pagetable == NULL ||
        shared_memory == NULL || task->trap_frame == NULL) {
        deallocate(task->kernel_stack, 0);
        deallocate(task->pagetable, 0);
        kfree(task);
        return NULL;
    }
    task->trap_frame->kernel_satp = (uint64)kernel_pagetable;
    task->trap_frame->epc = 0;
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
    map_result |= map_page(
        task->pagetable,
        (uint64)SHARED_MEMORY,
        (uint64)shared_memory,
        PTE_R | PTE_W | PTE_U
    );
    if (map_result != 0) {
        free_user_memory(task);
        kfree(task);
        return NULL;
    }
    task->state = RUNNABLE;
    task->pid = next_pid++;
    task->parent = parent;
    task->context.sp = (uint64)task->kernel_stack + PGSIZE;
    task->context.ra = (uint64)user_trap_return;
    task->stack.size = 0;
    task->stack.start = 0;
    strcpy(task->name, name, min(31UL, strlen(name)));
#ifdef TOY_RISCV_KERNEL_PRINT_TASK
    print_string("new task: ");
    print_string(task->name);
    print_string(", pid: ");
    print_int(task->pid, 10);
    print_string(", at ");
    print_int((uint64)task, 16);
    print_string("\n");
#endif
    task->shared_memory = shared_memory;
    return task;
}

int register_memory_section(struct task_struct *task, uint64 va, size_t size) {
    typedef struct memory_section memory_section;
    memory_section *tmp_data = kmalloc(sizeof(memory_section));
    struct single_linked_list_node *tmp = make_single_linked_list_node(tmp_data);
    if (tmp == NULL || tmp_data == NULL) {
        kfree(tmp_data);
        kfree(tmp);
        return -1;
    }
    tmp_data->start = va;
    tmp_data->size = size;
    push_tail(&(task->mem_sections), tmp);
    return 0;
}

int set_stack(struct task_struct *task) {
    void *stack = allocate_for_user(0);
    if (stack == NULL) return -1;
    task->stack.start = SHARED_MEMORY - PGSIZE;
    task->stack.size = PGSIZE;
    task->trap_frame->sp = SHARED_MEMORY;
    if (map_page(task->pagetable, SHARED_MEMORY - PGSIZE,
                 (uint64)stack, task->stack_permission) != 0) {
        deallocate(stack, 0);
        return -1;
    }
    return 0;
}

void clear_user_memory_space(struct task_struct *task) {
    pagetable_t pagetable = task->pagetable;
    stack_to_remove_next = task->kernel_stack;

    for (struct single_linked_list_node *node = task->mem_sections.head;
         node != NULL;
         node = node->next) {
        struct memory_section *mem_section = node->data;
        free_memory(pagetable, mem_section->start, mem_section->size);
        kfree(mem_section);
    }
    free_memory(pagetable, task->stack.start, task->stack.size);
    clear_single_linked_list(&(task->mem_sections));
}

void free_user_memory(struct task_struct *task) {
    pagetable_t pagetable = task->pagetable;
    deallocate(task->trap_frame, 0);
    deallocate(task->shared_memory, 0);
    clear_user_memory_space(task);
    free_pagetable(pagetable);
}

uint64 available_from(struct task_struct *task) {
    uint64 max = 0;
    for (struct single_linked_list_node *node = task->mem_sections.head;
         node != NULL;
         node = node->next) {
        struct memory_section *mem_section = node->data;
        if (mem_section->start + mem_section->size > max) {
            max = mem_section->start + mem_section->size;
        }
    }
    return max;
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

#ifdef TOY_RISCV_KERNEL_PRINT_TASK
void print_all_task_meta() {
    for (struct single_linked_list_node *node = all_tasks->head;
         node != NULL;
         node = node->next) {
        struct task_struct *task = node->data;
        print_task_meta(task);
    }
}
#endif

extern char init_program[];
extern char sh[];
extern char echo[];

void *elf_file(const char *name) {
    if (strcmp(name, "/init") == 0) {
        return init_program;
    } else if (strcmp(name, "/sh") == 0) {
        return sh;
    } else if (strcmp(name, "/echo") == 0) {
        return echo;
    } else {
        return NULL;
    }
}

void init_scheduler() {
    runnable_tasks = create_single_linked_list();
    all_tasks = create_single_linked_list();
    struct task_struct *init_task = new_task("init", NULL);
    if (init_task == NULL) {
        panic("init_scheduler: cannot create init task");
    }
    load_elf(init_program, init_task);
    if (set_stack(init_task) != 0) {
        panic("init_scheduler: cannot set stack for init task");
    }
    void *page = allocate_for_user(1);
    char *env = (char *)page;
    char **vectors = (char **)((uint64)page + PGSIZE);
    if (page == NULL) {
        panic("init_scheduler: cannot allocate page for init task");
    }
    uint64 va = available_from(init_task);
    if (register_memory_section(init_task, va, PGSIZE * 2) != 0) {
        panic("init_scheduler: cannot register memory section for init task");
    }
    if (map_page(init_task->pagetable, va, (uint64)page,
        PTE_R | PTE_W | PTE_U) != 0) {
        panic("init_scheduler: cannot map page for init task");
    }
    if (map_page(init_task->pagetable, va + PGSIZE, (uint64)page + PGSIZE,
        PTE_R | PTE_W | PTE_U) != 0) {
        panic("init_scheduler: cannot map page for init task");
    }
    strcpy(env, "init", 5);
    vectors[0] = (char *)va;
    vectors[1] = NULL;
    vectors[2] = NULL; // envp
    init_task->trap_frame->a0 = 1; // argc
    init_task->trap_frame->a1 = va + PGSIZE; // argv
    init_task->trap_frame->a2 = va + PGSIZE + 2 * sizeof(char *); // envp

    push_tail(all_tasks, make_single_linked_list_node(init_task));
    push_tail(runnable_tasks, make_single_linked_list_node(init_task));
    init = init_task;
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
            task->state = RUNNING;
            pop_head_without_free(runnable_tasks);
            switch_context(&now_context, &(task->context));
        }
        interrupt_on();
    }
}

struct single_linked_list_node *next_task_to_run() {
    if (runnable_tasks->size == 0) return NULL;
    struct single_linked_list_node *candidate_node = head_node(runnable_tasks);
    struct task_struct *candidate = candidate_node->data;
    while (candidate != NULL && candidate->state != RUNNABLE) {
        pop_head(runnable_tasks);
        candidate_node = head_node(runnable_tasks);
        candidate = candidate_node->data;
    }
    return candidate_node;
}

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

struct task_struct *new_task_with_data(
    const char *name,
    struct task_struct *parent,
    void *src_memory,
    size_t size
) {
    struct task_struct *task = new_task(name, parent);
    if (task == NULL) return NULL;
    typedef struct memory_section memory_section;
    memory_section *tmp_data = kmalloc(sizeof(memory_section));
    struct single_linked_list_node *tmp = make_single_linked_list_node(tmp_data);
    if (tmp == NULL || tmp_data == NULL) {
        free_user_memory(task);
        kfree(task);
        kfree(tmp_data);
        return NULL;
    }
    tmp_data->start = 0UL;
    tmp_data->size = size;
    push_tail(&(task->mem_sections), tmp);
    if (map_memory(task->pagetable,
                   src_memory,
                   size,
                   PTE_R | PTE_W | PTE_X | PTE_U) == 0) {
        free_user_memory(task);
        kfree(task);
        return NULL;
    }
    return task;
}

void test_scheduler() {
    struct task_struct *task1 = new_task_with_data("task1", NULL, program1, sizeof(program1));
    struct task_struct *task2 = new_task_with_data("task2", NULL, program2, sizeof(program2));
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
    struct task_struct *child = new_task(task->name, task);
    if (child == NULL) return -1;
    int map_result = 0;
    map_result |= copy_all_memory_with_pagetable(task, child);
    *(child->trap_frame) = *(task->trap_frame);
    child->trap_frame->a0 = 0; // fork() returns 0 in the child process
    child->trap_frame->epc += 4;
    if (map_result != 0) {
        free_user_memory(task);
        kfree(task);
        return -1;
    }
    push_tail(runnable_tasks, make_single_linked_list_node(child));
    push_tail(all_tasks, make_single_linked_list_node(child));
    return child->pid;
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
    for_each_node(all_tasks, reparent);
    if (task->parent->state == SLEEPING) {
        if (task->parent->channel == task->parent ||
            task->parent->channel == task) {
            task->state = DEAD;
            task->parent->state = RUNNABLE;
            task->parent->trap_frame->a0 = task->pid;
            // save in a1 temporarily
            task->parent->trap_frame->a1 = task->exit_status;
            push_tail(runnable_tasks,
                      make_single_linked_list_node(task->parent));
        }
    }
    yield();
    panic("exit_process: should not reach here\n");
}

uint64 exec_process(struct task_struct *task, int argv_size, int envp_size) {
    interrupt_off();
    char *ptr = task->shared_memory;
    char *const name = ptr;
    void *const elf = elf_file(name);
    if (elf == NULL) return -1; // no such file

    clear_user_memory_space(task);
    memset(task->trap_frame, 0, PGSIZE);
    while (*ptr != '\0') ptr++;
    while (*ptr == '\0') ptr++;
    char *argv = ptr;
    char *arguments = ptr;
    ptr += argv_size;
    char *envp = ptr;
    
    if (load_elf(elf, task) || set_stack(task) != 0) {
        exit_process(task, -1);
    }
    void *page = allocate_for_user(1);
    if (page == NULL) {
        exit_process(task, -1);
    }
    char *env = (char *)page;
    memcpy(env, task->shared_memory, PGSIZE);
    const uint64 va = available_from(task);
    uint64 *const argv_ptr = (uint64 *)((uint64)page + PGSIZE);
    uint64 *const envp_ptr = argv_ptr + argv_size + 1;
    for (int i = 0; i < argv_size; i++) {
        argv_ptr[i] = (uint64)va + ((uint64)argv - (uint64)task->shared_memory);
        while (*argv != '\0') argv++;
        argv++;
    }
    argv_ptr[argv_size] = NULL;
    for (int i = 0; i < envp_size; i++) {
        envp_ptr[i] = (uint64)va + ((uint64)envp - (uint64)task->shared_memory);
        while (*envp != '\0') envp++;
        envp++;
    }
    envp_ptr[envp_size] = NULL;

    // Set the arguments for the new process
    task->trap_frame->a0 = argv_size;
    task->trap_frame->a1 = (uint64)va + ((uint64)argv_ptr - (uint64)env);
    task->trap_frame->a2 = (uint64)va + ((uint64)envp_ptr - (uint64)env);

    // Register and map the argv and envp
    if (register_memory_section(task, va, PGSIZE * 2) != 0) {
        exit_process(task, -1);
    }
    if (map_page(task->pagetable, va, (uint64)page,
        PTE_R | PTE_W | PTE_U) != 0) {
        exit_process(task, -1);
    }
    if (map_page(task->pagetable, va + PGSIZE, (uint64)page + PGSIZE,
        PTE_R | PTE_W | PTE_U) != 0) {
        exit_process(task, -1);
    }

    // change the name of the process
    int arguments_count = 1;
    for (int i = 0; i < 31; i++) {
        task->name[i] = '\0';
    }
    for (int i = 0; i < 31; i++) {
        if (*arguments != '\0') {
            task->name[i] = *arguments;
            arguments++;
        } else {
            arguments_count++;
            if (arguments_count >= argv_size) break;
            task->name[i] = ' ';
            while (*arguments == '\0') arguments++;
        }
    }
    task->name[31] = '\0';
    interrupt_on();
    user_trap_return();
    panic("exec_process: should not reach here\n");
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
uint64 sys_yield(struct task_struct *task);
uint64 sys_power_off(struct task_struct *task);
uint64 sys_put_char(struct task_struct *task);
uint64 sys_get_char(struct task_struct *task);

#define SYSCALL_FORK        1
#define SYSCALL_EXEC        2
#define SYSCALL_EXIT        3
#define SYSCALL_WAIT        4
#define SYSCALL_WAIT_PID    5
#define SYSCALL_SEND_SIGNAL 6
#define SYSCALL_YIELD       7
#define SYSCALL_POWER_OFF   8
#define SYSCALL_PUT_CHAR    9
#define SYSCALL_GET_CHAR    10

static uint64 (*syscalls[])(struct task_struct *) = {
    [SYSCALL_FORK]        = sys_fork,
    [SYSCALL_EXEC]        = sys_exec,
    [SYSCALL_EXIT]        = sys_exit,
    [SYSCALL_WAIT]        = sys_wait,
    [SYSCALL_WAIT_PID]    = sys_wait_pid,
    [SYSCALL_SEND_SIGNAL] = sys_send_signal,
    [SYSCALL_YIELD]       = sys_yield,
    [SYSCALL_POWER_OFF]   = sys_power_off,
    [SYSCALL_PUT_CHAR]    = sys_put_char,
    [SYSCALL_GET_CHAR]    = sys_get_char,
};

void syscall() {
    struct task_struct *current = current_task();
    if (current == NULL) panic("syscall: no task running!");
    int id = current->trap_frame->a7;
    if (id > 0 && id < sizeof(syscalls) / sizeof(syscalls[0])) {
        // The return value should be returned to user
        current->trap_frame->a0 = syscalls[id](current);
        current->trap_frame->epc += 4;
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
    int argv_size = task->trap_frame->a3;
    int envp_size = task->trap_frame->a4;
    return exec_process(task, argv_size, envp_size);
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

uint64 sys_yield(struct task_struct *task) {
    yield();
    return 0;
}

uint64 sys_power_off(struct task_struct *task) {
    if (task->pid != 1) return -1;
    *(uint32 *)VIRT_TEST = 0x5555;
    return 0;
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

/** Trap handlers for specific causes */

inline int within_stack_range(uint64 addr) {
    return addr >= MIN_STACK_ADDR && addr < SHARED_MEMORY;
}

int enlarge_stack_by_a_page(struct task_struct *task) {
    void *page = allocate_for_user(0);
    if (page == NULL) return -1;
    uint64 original_start = task->stack.start;
    uint64 new_start = original_start - PGSIZE;
    if (map_page(task->pagetable, new_start, (uint64)page,
                 task->stack_permission) != 0) {
        deallocate(page, 0);
        return -1;
    }
    task->stack.start -= PGSIZE;
    task->stack.size += PGSIZE;
    return 0;
}

int try_enlarge_stack(struct task_struct *task, uint64 addr) {
    if (within_stack_range(addr)) {
        uint64 new_start = PGROUNDDOWN(addr);
        if (new_start < task->stack.start) {
            size_t times_of_enlargement =
                (task->stack.start - new_start) / PGSIZE;
            for (size_t i = 0; i < times_of_enlargement; i++) {
                if (enlarge_stack_by_a_page(task) != 0) return -1;
            }
        }
        return 0;
    } else {
        return -1;
    }
}

void handle_load_page_fault(struct task_struct *task) {
    uint64 addr = read_stval();
    if (try_enlarge_stack(task, addr) == 0) return;
    print_string("Load page fault at ");
    print_int(addr, 16);
    print_string(", pid ");
    print_int(task->pid, 10);
    print_string("\n");
    exit_process(task, -1);
}

void handle_store_page_fault(struct task_struct *task) {
    uint64 addr = read_stval();
    if (try_enlarge_stack(task, addr) == 0) return;
    print_string("Store page fault at ");
    print_int(addr, 16);
    print_string(", pid ");
    print_int(task->pid, 10);
    print_string("\n");
    exit_process(task, -1);
}
