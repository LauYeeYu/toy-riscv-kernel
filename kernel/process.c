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
#include "trap.h"
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
    if (task->pagetable == NULL) {
        deallocate(task->kernel_stack, 0);
        kfree(task);
        return NULL;
    }
    task->memory_size = init_virtual_memory_for_user(
        task->pagetable,
        src_memory,
        size
    );
    if (task->memory_size == 0) {
        deallocate(task->kernel_stack, 0);
        deallocate(task->pagetable, 0);
        kfree(task);
        return NULL;
    }
    void *user_stack = allocate(0);
    task->memory_size += PGSIZE;
    task->trap_frame = allocate(0);
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
    deallocate(task->pagetable, 0);
}

/** Scheduler part */

struct task_struct *running_task = NULL;
struct single_linked_list *runnable_tasks = NULL;
struct context now_context;

struct task_struct *current_task() {
    return running_task;
}

void init_scheduler() {
    runnable_tasks = create_single_linked_list();
    //TODO: add /init here
}

void scheduler() {
    for (;;) {
        interrupt_off();
        if (running_task != NULL) {
            panic("scheduler: trying to run a task while another task is running");
        }
        if (runnable_tasks->size > 0) {
            struct task_struct *task = head(runnable_tasks);
            running_task = task;
            pop_head(runnable_tasks);
            switch_context(&now_context, &(running_task->context));
        }
        interrupt_on();
    }

}

void yield() {
    struct context *old_context = &now_context;
    if (running_task != NULL) {
        running_task->state = RUNNABLE;
        push_tail(runnable_tasks, make_single_linked_list_node(running_task));
        old_context = &(running_task->context);
    }

    running_task = head(runnable_tasks);
    pop_head(runnable_tasks);
    running_task->state = RUNNING;
    switch_context(old_context, &(running_task->context));
}
