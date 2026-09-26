#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_MAX 32
#define TASK_STACK_SIZE 4096

typedef enum
{
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_TERMINATED
} task_state_t;

typedef struct cpu_context
{
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

} cpu_context_t;
typedef struct task
{
    uint32_t pid;

    task_state_t state;

    void (*entry)(void);

    uint32_t stack_base;
    uint32_t stack_top;
    uint32_t stack_pointer;
    /*
 * Stack pointer used by the preemptive
 * IRQ context-switch path.
 */
uint32_t irq_stack_pointer;
int irq_started;

/*
 * Set to 1 when the task has completed its work.
 * The timer/scheduler path will handle termination.
 */
int finished;

int is_idle;

cpu_context_t context;
} task_t;


void task_init(void);

int task_create(void (*entry)(void));

int task_destroy(uint32_t pid);

task_t* task_get(uint32_t pid);

uint32_t task_get_count(void);

uint32_t task_get_kernel_stack(void);

task_t* task_get_at(uint32_t index);

int task_test(void);
int task_switch_test(void);
int task_preempt_test(void);
int task_lifecycle_test(void);
int task_scheduler_stress_test(void);

void task_block(void);
void task_yield(void);
void task_start(task_t* task);
void task_start_preemptive(task_t* task);
void task_exit(void);
void task_finish(void);
void task_reap_terminated(void);

uint32_t task_get_idle_irq_stack(void);

#endif
