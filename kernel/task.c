#include "task.h"
#include "heap.h"
#include "scheduler.h"
#include "context.h"
#include "terminal.h"
#include "../hindsight/event.h"

static task_t* task_table[TASK_MAX];
static uint32_t next_pid = 1;
static uint32_t kernel_stack_pointer = 0;
uint32_t kernel_base_pointer = 0;
static uint32_t task_count = 0;
uint32_t task_get_kernel_stack(void)
{
    return kernel_stack_pointer;
}
/*
 * Kernel stack saved when the first task is started.
 *
 * When the last task finishes, we switch back
 * to this stack and return to task_start().
 */

/*
 * Forward declaration.
 */
/*
 * Dedicated stack for the kernel idle context.
 *
 * This is separate from the stack saved by
 * task_start_preemptive().
 */
static uint8_t kernel_idle_stack[TASK_STACK_SIZE]
    __attribute__((aligned(16)));

static uint32_t kernel_idle_irq_stack_pointer = 0;

static void kernel_idle(void)
{
    /*
     * We are about to perform a context transition.
     *
     * Do NOT enable interrupts until the kernel stack
     * has been restored and the RET has completed.
     */
    __asm__ volatile ("cli");

    terminal_write(" [IDLE] ");

    scheduler_clear_current();

    terminal_write(" [CURRENT CLEARED] ");

    task_reap_terminated();

    terminal_write(" [TASKS REAPED] ");

    uint32_t kernel_stack = task_get_kernel_stack();

    terminal_write(" [RETURNING] ");

    if (kernel_stack != 0)
    {
        /*
         * context_return_to_stack() restores:
         *
         *   ESP
         *   EBP
         *
         * and RETs directly back into the kernel.
         */
        context_return_to_stack(kernel_stack);
    }

    /*
     * We should only reach here if there is no kernel
     * context to return to.
     */
    __asm__ volatile ("sti");

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}
static void kernel_idle_init(void)
{
    uint32_t* stack =
        (uint32_t*)
        (kernel_idle_stack + TASK_STACK_SIZE);

    /*
     * Build the same stack frame expected by
     * context_start():
     *
     * EFLAGS
     * CS
     * EIP
     * EAX
     * ECX
     * EDX
     * EBX
     * ESP
     * EBP
     * ESI
     * EDI
     */

    *--stack = 0x00000202;
    *--stack = 0x00000008;
    *--stack = (uint32_t)kernel_idle;

    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;
    *--stack = 0;

    kernel_idle_irq_stack_pointer =
        (uint32_t)stack;
}
static void task_trampoline(void);

/*
 * Every newly created task starts here.
 */
static void task_trampoline(void)
{
    task_t* task =
        scheduler_get_current();

    if (task != 0 && task->entry != 0)
    {
        task->entry();
    }

    task_exit();

    /*
     * task_exit() should never return.
     */
    while (1)
    {
        __asm__ volatile ("hlt");
    }
}

void task_init(void)
{
    for (uint32_t i = 0; i < TASK_MAX; i++)
        task_table[i] = 0;

    next_pid = 1;
    task_count = 0;
    kernel_stack_pointer = 0;
kernel_idle_init();
}

int task_create(void (*entry)(void))
{
    if (entry == 0)
        return -1;

    if (task_count >= TASK_MAX)
        return -1;

    uint32_t slot = TASK_MAX;

    for (uint32_t i = 0; i < TASK_MAX; i++)
    {
        if (task_table[i] == 0)
        {
            slot = i;
            break;
        }
    }

    if (slot >= TASK_MAX)
        return -1;

    task_t* task =
        (task_t*)kmalloc(sizeof(task_t));

    if (task == 0)
        return -1;

    void* stack =
        kmalloc(TASK_STACK_SIZE);

    if (stack == 0)
    {
        kfree(task);
        return -1;
    }

    task->pid = next_pid++;
    task->state = TASK_READY;
    task->entry = entry;

    task->stack_base =
        (uint32_t)stack;

    task->stack_top =
        (uint32_t)stack + TASK_STACK_SIZE;
/*
 * Prepare a separate IRQ-compatible context.
 *
 * Keep it away from the cooperative context
 * at the top of the same stack.
 *
 * The task will begin preemptive execution
 * from this frame later.
 */
uint32_t* irq_stack =
    (uint32_t*)(
        task->stack_top - 64
    );

/*
 * Build the IRETD frame first.
 *
 * IRETD expects:
 *
 * EIP
 * CS
 * EFLAGS
 *
 * Because the stack grows downward,
 * push them in reverse order.
 */
*--irq_stack = 0x00000202;          /* EFLAGS: IF enabled */
*--irq_stack = 0x00000008;          /* Kernel CS */
*--irq_stack =
    (uint32_t)task_trampoline;     /* EIP */

/*
 * Build the PUSHA-compatible register frame.
 *
 * POPA expects:
 *
 * EDI
 * ESI
 * EBP
 * saved ESP
 * EBX
 * EDX
 * ECX
 * EAX
 */
*--irq_stack = 0;                  /* EAX */
*--irq_stack = 0;                  /* ECX */
*--irq_stack = 0;                  /* EDX */
*--irq_stack = 0;                  /* EBX */
*--irq_stack = 0;                  /* fake ESP */
*--irq_stack = 0;                  /* EBP */
*--irq_stack = 0;                  /* ESI */
*--irq_stack = 0;                  /* EDI */

task->irq_stack_pointer =
    (uint32_t)irq_stack;

task->irq_started = 0;
task->finished = 0;
task->is_idle = 0;
    /*
     * Build the initial stack expected by
     * context_switch().
     *
     * context_switch() expects:
     *
     * ESP -> EDI
     *        ESI
     *        EBX
     *        EBP
     *        RETURN ADDRESS
     */

    uint32_t* stack_pointer =
        (uint32_t*)task->stack_top;

    /*
     * Return address.
     */
    *--stack_pointer =
        (uint32_t)task_trampoline;

    /*
     * Callee-saved registers.
     */
    *--stack_pointer = 0;   /* EBP */
    *--stack_pointer = 0;   /* EBX */
    *--stack_pointer = 0;   /* ESI */
    *--stack_pointer = 0;   /* EDI */

    task->stack_pointer =
        (uint32_t)stack_pointer;

    /*
     * Initial context.
     */
    task->context.edi = 0;
    task->context.esi = 0;
    task->context.ebp = 0;
    task->context.esp =
        (uint32_t)stack_pointer;
    task->context.ebx = 0;
    task->context.edx = 0;
    task->context.ecx = 0;
    task->context.eax = 0;

    task_table[slot] = task;
    task_count++;

    /*
     * Hindsight:
     * Record successful task creation using the real PID.
     */
    hindsight_event_record(
        HINDSIGHT_EVENT_TASK_CREATE,
        task->pid,
        0,
        0,
        0
    );

    return (int)task->pid;
}

int task_destroy(uint32_t pid)
{
    task_t* task =
        task_get(pid);

    if (task == 0)
        return 0;

    /*
     * Never free the stack of a currently
     * running task.
     */
    if (scheduler_get_current() == task)
        return 0;

    for (uint32_t i = 0; i < TASK_MAX; i++)
    {
        if (task_table[i] == task)
        {
            task_table[i] = 0;

            kfree((void*)task->stack_base);
            kfree(task);

            if (task_count > 0)
                task_count--;

            return 1;
        }
    }

    return 0;
}

task_t* task_get(uint32_t pid)
{
    for (uint32_t i = 0; i < TASK_MAX; i++)
    {
        if (task_table[i] != 0 &&
            task_table[i]->pid == pid)
        {
            return task_table[i];
        }
    }

    return 0;
}

uint32_t task_get_count(void)
{
    return task_count;
}

task_t* task_get_at(uint32_t index)
{
    if (index >= TASK_MAX)
        return 0;

    return task_table[index];
}

/*
 * Cooperative yield.
 */
void task_yield(void)
{
    task_t* current =
        scheduler_get_current();

    task_t* next =
        scheduler_select_next();

    if (next == 0)
        return;

    if (current == next)
        return;

    if (current == 0)
    {
        /*
         * This case is normally handled by
         * task_start(), but keep it safe.
         */
        context_switch(
            &kernel_stack_pointer,
            next->context.esp
        );

        return;
    }

    /*
     * Save current task's ESP and load
     * the next task's ESP.
     */
    context_switch(
        &current->context.esp,
        next->context.esp
    );

    /*
     * When this task is scheduled again,
     * execution continues here.
     */
}

/*
 * Start the first task from the kernel.
 */
void task_start(task_t* task)
{
    if (task == 0)
        return;

    /*
     * Save the kernel's current ESP.
     */
    context_switch(
        &kernel_stack_pointer,
        task->context.esp
    );

    /*
     * Execution returns here when all tasks
     * have finished.
     */
}
/*
 * Start a task using its prepared IRQ context.
 *
 * This is the entry point for preemptive
 * multitasking.
 */
__attribute__((noinline))
void task_start_preemptive(task_t* task)
{
    if (task == 0)
        return;

    /*
     * The task must already have been selected
     * by the scheduler.
     */
    if (scheduler_get_current() != task)
        return;

    /*
     * Start execution using the IRQ-compatible
     * stack prepared by task_create().
     */
    task->irq_started = 1;
    context_start(
        &kernel_stack_pointer,
        task->irq_stack_pointer
    );
}
/*
 * Terminate the current task.
 */
void task_finish(void)
{
    task_t* current =
        scheduler_get_current();

    if (current == 0)
        return;

    /*
     * Mark the task as finished immediately.
     *
     * The task is still physically executing until
     * the next timer interrupt, so its stack is NOT
     * freed here.
     */
    current->finished = 1;

    /*
     * Hindsight:
     * Record that this task has terminated.
     */
    hindsight_event_record(
        HINDSIGHT_EVENT_TASK_EXIT,
        current->pid,
        0,
        0,
        0
    );

    current->state = TASK_TERMINATED;
}
void task_exit(void)
{
    task_t* current =
        scheduler_get_current();

    if (current == 0)
    {
        while (1)
        {
            __asm__ volatile ("hlt");
        }
    }

    /*
     * Hindsight:
     * Record that this task is exiting.
     */
    hindsight_event_record(
        HINDSIGHT_EVENT_TASK_EXIT,
        current->pid,
        0,
        0,
        0
    );

    /*
     * A terminated task must never be selected
     * again by the scheduler.
     */
    current->state =
        TASK_TERMINATED;

    /*
     * Look for another READY task.
     */
    task_t* next =
        scheduler_select_next();

    if (next != 0 &&
        next != current)
    {
        /*
         * Switch away from the terminated task.
         *
         * We deliberately do NOT free its stack
         * here because we are still executing on it.
         */
        context_switch(
            &current->context.esp,
            next->context.esp
        );

        /*
         * This task should not normally reach here.
         */
        while (1)
        {
            __asm__ volatile ("hlt");
        }
    }

    /*
     * No READY tasks remain.
     *
     * Return to the kernel context saved by
     * task_start_preemptive().
     */
    if (kernel_stack_pointer != 0)
    {
        uint32_t unused_stack;

        context_switch(
            &unused_stack,
            kernel_stack_pointer
        );
    }

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}
void task_reap_terminated(void)
{
    task_t* current =
        scheduler_get_current();

    for (uint32_t i = 0; i < TASK_MAX; i++)
    {
        task_t* task =
            task_table[i];

        if (task == 0)
            continue;

        /*
         * Never reclaim the task whose stack
         * we are currently using.
         */
        if (task == current)
            continue;

       if (task->state == TASK_TERMINATED ||
       task->finished)
        {
            task_table[i] = 0;

            kfree(
                (void*)task->stack_base
            );

            kfree(task);

            if (task_count > 0)
                task_count--;
        }
    }
}
/* ------------------------------------------------ */
/* Task Manager Test                                */
/* ------------------------------------------------ */

static void task_test_entry_a(void)
{
}

static void task_test_entry_b(void)
{
}

int task_test(void)
{
    uint32_t initial_count =
        task_get_count();

    int pid_a =
        task_create(task_test_entry_a);

    if (pid_a <= 0)
        return 0;

    int pid_b =
        task_create(task_test_entry_b);

    if (pid_b <= 0)
    {
        task_destroy((uint32_t)pid_a);
        return 0;
    }

    if (pid_a == pid_b)
    {
        task_destroy((uint32_t)pid_a);
        task_destroy((uint32_t)pid_b);
        return 0;
    }

    task_t* task_a =
        task_get((uint32_t)pid_a);

    task_t* task_b =
        task_get((uint32_t)pid_b);

    if (task_a == 0 || task_b == 0)
    {
        task_destroy((uint32_t)pid_a);
        task_destroy((uint32_t)pid_b);
        return 0;
    }

    if (task_a->state != TASK_READY ||
        task_b->state != TASK_READY)
    {
        task_destroy((uint32_t)pid_a);
        task_destroy((uint32_t)pid_b);
        return 0;
    }

    if (task_a->stack_base ==
        task_b->stack_base)
    {
        task_destroy((uint32_t)pid_a);
        task_destroy((uint32_t)pid_b);
        return 0;
    }

    if (task_a->stack_pointer ==
        task_b->stack_pointer)
    {
        task_destroy((uint32_t)pid_a);
        task_destroy((uint32_t)pid_b);
        return 0;
    }

    if (task_get_count() !=
        initial_count + 2)
    {
        task_destroy((uint32_t)pid_a);
        task_destroy((uint32_t)pid_b);
        return 0;
    }

    if (task_a->context.esp !=
        task_a->stack_pointer)
    {
        task_destroy((uint32_t)pid_a);
        task_destroy((uint32_t)pid_b);
        return 0;
    }

    if (task_b->context.esp !=
        task_b->stack_pointer)
    {
        task_destroy((uint32_t)pid_a);
        task_destroy((uint32_t)pid_b);
        return 0;
    }
if (task_a->irq_stack_pointer == 0 ||
    task_b->irq_stack_pointer == 0)
{
    task_destroy((uint32_t)pid_a);
    task_destroy((uint32_t)pid_b);
    return 0;
}

if (task_a->irq_stack_pointer ==
    task_b->irq_stack_pointer)
{
    task_destroy((uint32_t)pid_a);
    task_destroy((uint32_t)pid_b);
    return 0;
}

    if (!task_destroy((uint32_t)pid_a))
    {
        task_destroy((uint32_t)pid_b);
        return 0;
    }

    if (!task_destroy((uint32_t)pid_b))
        return 0;

    if (task_get_count() !=
        initial_count)
    {
        return 0;
    }

    if (task_get((uint32_t)pid_a) != 0 ||
        task_get((uint32_t)pid_b) != 0)
    {
        return 0;
    }

    return 1;
}

/* ------------------------------------------------ */
/* Context Switch Test                              */
/* ------------------------------------------------ */

static volatile uint32_t switch_a_count = 0;
static volatile uint32_t switch_b_count = 0;

static void switch_test_task_a(void)
{
    for (uint32_t i = 0; i < 3; i++)
    {
        switch_a_count++;
        task_yield();
    }
}

static void switch_test_task_b(void)
{
    for (uint32_t i = 0; i < 3; i++)
    {
        switch_b_count++;
        task_yield();
    }
}

int task_switch_test(void)
{
    switch_a_count = 0;
    switch_b_count = 0;

    uint32_t initial_count =
        task_get_count();

    int pid_a =
        task_create(switch_test_task_a);

    if (pid_a <= 0)
        return 0;

    int pid_b =
        task_create(switch_test_task_b);

    if (pid_b <= 0)
    {
        task_destroy((uint32_t)pid_a);
        return 0;
    }

    /*
     * Select the first task.
     */
    task_t* first =
        scheduler_select_next();

    if (first == 0)
    {
        task_destroy((uint32_t)pid_a);
        task_destroy((uint32_t)pid_b);
        return 0;
    }

    /*
     * Start execution.
     *
     * This does not return until both tasks
     * have terminated.
     */
    task_start(first);

    /*
     * Both tasks should have executed exactly
     * three times.
     */
    if (switch_a_count != 3 ||
        switch_b_count != 3)
    {
        return 0;
    }

    /*
     * Both tasks should have terminated.
     */
    task_t* task_a =
        task_get((uint32_t)pid_a);

    task_t* task_b =
        task_get((uint32_t)pid_b);

    if (task_a == 0 ||
        task_b == 0)
    {
        return 0;
    }

    if (task_a->state != TASK_TERMINATED ||
        task_b->state != TASK_TERMINATED)
    {
        return 0;
    }

    /*
     * Clean up terminated tasks.
     *
     * scheduler_get_current() may still point
     * to the last terminated task, so clear it
     * through scheduler_init().
     */
    scheduler_init();

    if (!task_destroy((uint32_t)pid_a))
        return 0;

    if (!task_destroy((uint32_t)pid_b))
        return 0;

    if (task_get_count() != initial_count)
        return 0;

    return 1;
}
/* ------------------------------------------------ */
/* Preemptive Multitasking Test                    */
/* ------------------------------------------------ */

static volatile uint32_t preempt_a_count = 0;
static volatile uint32_t preempt_b_count = 0;

static void preempt_test_task_a(void)
{
    while (1)
    {
        preempt_a_count++;

        /*
         * Periodically display that Task A
         * is receiving CPU time.
         */
        if ((preempt_a_count % 10000000) == 0)
        {
            terminal_write("A");
        }
    }
}

static void preempt_test_task_b(void)
{
    while (1)
    {
        preempt_b_count++;

        /*
         * Periodically display that Task B
         * is receiving CPU time.
         */
        if ((preempt_b_count % 10000000) == 0)
        {
            terminal_write("B");
        }
    }
}

int task_preempt_test(void)
{
    preempt_a_count = 0;
    preempt_b_count = 0;

    int pid_a =
        task_create(preempt_test_task_a);

    if (pid_a <= 0)
        return 0;

    int pid_b =
        task_create(preempt_test_task_b);

    if (pid_b <= 0)
    {
        task_destroy((uint32_t)pid_a);
        return 0;
    }

/*
 * Select the first task through the scheduler.
 */
task_t* first =
    scheduler_select_next();

if (first == 0)
{
    task_destroy((uint32_t)pid_a);
    task_destroy((uint32_t)pid_b);
    return 0;
}

/*
 * Start the task selected by the scheduler.
 */
task_start_preemptive(first);
    /*
     * Start Task A.
     *
     * From this point onward neither task
     * calls task_yield().
     *
     * The PIT must preempt them.
     */
    task_start_preemptive(first);

    /*
     * We should never reach here during
     * this first preemption demonstration.
     */
    return 1;
}
/* ============================================================
 * TASK LIFECYCLE TEST
 * ============================================================ */

static volatile uint32_t lifecycle_a_count = 0;
static volatile uint32_t lifecycle_b_count = 0;

static void lifecycle_test_task_a(void)
{
    terminal_write(" [A START] ");

    lifecycle_a_count++;

    task_finish();

    terminal_write(" [A FINISHED] ");

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}

static void lifecycle_test_task_b(void)
{
    terminal_write(" [B START] ");

    lifecycle_b_count++;

    task_finish();

    terminal_write(" [B FINISHED] ");

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}
int task_lifecycle_test(void)
{
    lifecycle_a_count = 0;
    lifecycle_b_count = 0;

    /*
     * Record the task count before the test.
     */
    uint32_t before_count =
        task_get_count();

    terminal_write(" [BEFORE A] ");

    int pid_a =
        task_create(lifecycle_test_task_a);

    terminal_write(" [AFTER A] ");

    if (pid_a <= 0)
    {
        terminal_write(" [A CREATE FAILED] ");
        return 0;
    }

    terminal_write(" [BEFORE B] ");

    int pid_b =
        task_create(lifecycle_test_task_b);

    terminal_write(" [AFTER B] ");

    if (pid_b <= 0)
    {
        terminal_write(" [B CREATE FAILED] ");

        scheduler_clear_current();
        task_destroy((uint32_t)pid_a);

        return 0;
    }

    /*
     * Both tasks should now exist.
     */
    if (task_get_count() != before_count + 2)
    {
        terminal_write(" [COUNT CHECK FAILED] ");

        scheduler_clear_current();

        task_destroy((uint32_t)pid_a);
        task_destroy((uint32_t)pid_b);

        return 0;
    }

terminal_write(" [SELECTING TASK] ");

__asm__ volatile ("cli");

task_t* first = scheduler_select_next();

if (first == 0)
{
    __asm__ volatile ("sti");

    terminal_write(" [NO READY TASK] ");

    scheduler_clear_current();

    task_destroy((uint32_t)pid_a);
    task_destroy((uint32_t)pid_b);

    return 0;
}

terminal_write(" [TASK SELECTED] ");
terminal_write(" [STARTING PREEMPTION] ");

task_start_preemptive(first);

/*
 * We normally do not reach this point until the task
 * system has restored the kernel context.
 */
__asm__ volatile ("sti");
    terminal_write(" [RETURNED TO KERNEL] ");

    /*
     * Both test tasks must have executed.
     */
    if (lifecycle_a_count == 0)
    {
        terminal_write(" [A DID NOT RUN] ");
        return 0;
    }

    if (lifecycle_b_count == 0)
    {
        terminal_write(" [B DID NOT RUN] ");
        return 0;
    }

    /*
     * The idle/reaper path should have reclaimed
     * both test tasks.
     */
uint32_t after_count = task_get_count();

terminal_write(" [BEFORE=");
terminal_write_dec(before_count);
terminal_write("] ");

terminal_write(" [AFTER=");
terminal_write_dec(after_count);
terminal_write("] ");

if (after_count == before_count)
    terminal_write(" [COUNT RESTORED] ");
else
    terminal_write(" [COUNT MISMATCH] ");
if (after_count != before_count)
{
    terminal_write(" [REAP CHECK FAILED] ");
    return 0;
}
    /*
     * Verify both PIDs are gone.
     */
    if (task_get((uint32_t)pid_a) != 0)
    {
        terminal_write(" [A STILL EXISTS] ");
        return 0;
    }

    if (task_get((uint32_t)pid_b) != 0)
    {
        terminal_write(" [B STILL EXISTS] ");
        return 0;
    }

    terminal_write(" [LIFECYCLE COMPLETE] ");

    return 1;
}
/* ============================================================
 * SCHEDULER STRESS TEST
 * ============================================================ */
#define STRESS_WORK 100000

static volatile uint32_t stress_a_count = 0;
static volatile uint32_t stress_b_count = 0;
static volatile uint32_t stress_c_count = 0;


static void scheduler_stress_task_a(void)
{
    terminal_write(" [A START] ");

    while (stress_a_count < 100000)
    {
        stress_a_count++;
    }

    terminal_write(" [A DONE] ");

    task_finish();

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}

static void scheduler_stress_task_b(void)
{
    terminal_write(" [B START] ");

    while (stress_b_count < 100000)
    {
        stress_b_count++;
    }

    terminal_write(" [B DONE] ");

    task_finish();

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}

static void scheduler_stress_task_c(void)
{
    terminal_write(" [C START] ");

    while (stress_c_count < 100000)
    {
        stress_c_count++;
    }

    terminal_write(" [C DONE] ");

    task_finish();

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}
int task_scheduler_stress_test(void)
{
    stress_a_count = 0;
    stress_b_count = 0;
    stress_c_count = 0;

    uint32_t before_count =
        task_get_count();

    int pid_a =
        task_create(scheduler_stress_task_a);

    if (pid_a <= 0)
        return 0;

    int pid_b =
        task_create(scheduler_stress_task_b);

    if (pid_b <= 0)
    {
        task_destroy((uint32_t)pid_a);
        return 0;
    }

    int pid_c =
        task_create(scheduler_stress_task_c);

    if (pid_c <= 0)
    {
        task_destroy((uint32_t)pid_a);
        task_destroy((uint32_t)pid_b);
        return 0;
    }

    /*
     * Select the first task.
     */
    __asm__ volatile ("cli");

    task_t* first =
        scheduler_select_next();

    if (first == 0)
    {
        __asm__ volatile ("sti");

        scheduler_clear_current();

        task_destroy((uint32_t)pid_a);
        task_destroy((uint32_t)pid_b);
        task_destroy((uint32_t)pid_c);

        return 0;
    }

    /*
     * Start the first task.
     *
     * From here the PIT must perform
     * the actual preemptive switching.
     */
    task_start_preemptive(first);

    /*
     * Execution returns here after the
     * idle/reaper path restores the kernel.
     */
    __asm__ volatile ("sti");

    /*
     * Every task must have received CPU time.
     */
    if (stress_a_count == 0 ||
        stress_b_count == 0 ||
        stress_c_count == 0)
    {
        return 0;
    }

    /*
     * Each task should have completed
     * its entire workload.
     */
    if (stress_a_count != STRESS_WORK ||
        stress_b_count != STRESS_WORK ||
        stress_c_count != STRESS_WORK)
    {
        return 0;
    }

    /*
     * The idle/reaper path should have
     * removed all three tasks.
     */
    if (task_get_count() != before_count)
        return 0;

    /*
     * PIDs must no longer exist.
     */
    if (task_get((uint32_t)pid_a) != 0)
        return 0;

    if (task_get((uint32_t)pid_b) != 0)
        return 0;

    if (task_get((uint32_t)pid_c) != 0)
        return 0;

    return 1;
}
uint32_t task_get_idle_irq_stack(void)
{
    return kernel_idle_irq_stack_pointer;
}
