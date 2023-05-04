#include "process.h"

#include "defs.h"
#include "mem_manage.h"
#include "panic.h"
#include "riscv.h"
#include "single_linked_list.h"
#include "switch.h"

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
            panic("scheduler: trying to run a task while another task is running\n");
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
    if (running_task != NULL) {
        running_task->state = RUNNABLE;
        push_tail(runnable_tasks, make_single_linked_list_node(running_task));
    }

    running_task = head(runnable_tasks);
    pop_head(runnable_tasks);
    running_task->state = RUNNING;
    switch_context(&(running_task->context), &now_context);
}
