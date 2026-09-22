#include "timer.h"
#include "scheduler.h"
#include "task.h"

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40

#define PIT_BASE_FREQUENCY 1193182
#define TIMER_FREQUENCY 100

static volatile uint32_t timer_ticks = 0;

static void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

void timer_init(void)
{
    uint16_t divisor =
        PIT_BASE_FREQUENCY / TIMER_FREQUENCY;

    /*
     * Channel 0
     *
     * Access:
     * low byte / high byte
     *
     * Mode 3:
     * square wave generator
     */
    outb(
        PIT_COMMAND_PORT,
        0x36
    );

    outb(
        PIT_CHANNEL0_PORT,
        (uint8_t)(divisor & 0xFF)
    );

    outb(
        PIT_CHANNEL0_PORT,
        (uint8_t)(divisor >> 8)
    );
}

/*
 * IRQ0 timer handler.
 *
 * The timer:
 *
 *   1. counts the tick
 *   2. acknowledges the PIC
 *   3. saves the interrupted task context
 *      only after that task has actually started
 *   4. handles terminated tasks
 *   5. selects the next READY task
 *   6. returns the correct IRQ stack
 */
uint32_t timer_interrupt_handler(
    uint32_t* stack_pointer
)
{
    timer_ticks++;

    /*
     * Acknowledge IRQ0.
     */
    outb(0x20, 0x20);

    task_t* current =
        scheduler_get_current();

    /*
     * No task is currently scheduled.
     * Continue using the interrupted stack.
     */
    if (current == 0)
    {
        return (uint32_t)stack_pointer;
    }

    /*
     * IMPORTANT:
     *
     * A task that has never started does NOT have
     * an interrupted CPU context yet.
     *
     * Its irq_stack_pointer still contains the
     * artificial startup frame created by
     * task_create().
     *
     * Only overwrite it after the task has actually
     * entered execution.
     */
    if (current->irq_started)
    {
        current->irq_stack_pointer =
            (uint32_t)stack_pointer;
    }

    /*
     * If the task finished its work, mark it
     * terminated before selecting another task.
     */
    if (current->finished)
    {
        current->state = TASK_TERMINATED;
    }

    /*
     * Select the next READY task.
     */
    task_t* next =
        scheduler_select_next();

    /*
     * No READY task exists.
     */
    if (next == 0)
    {
        /*
         * If the current task has terminated,
         * enter the idle context.
         */
        if (current->state == TASK_TERMINATED)
        {
            return task_get_idle_irq_stack();
        }

        /*
         * Otherwise continue the interrupted task.
         */
        return (uint32_t)stack_pointer;
    }

    /*
     * The scheduler selected the same task.
     *
     * Continue from the current interrupt frame.
     */
    if (next == current)
    {
        return (uint32_t)stack_pointer;
    }

    /*
     * Resume the next task.
     *
     * For a task that has never started, this is
     * the artificial startup frame created by
     * task_create().
     *
     * For an already-running task, this is the
     * stack saved by a previous timer interrupt.
     */
    return next->irq_stack_pointer;
}

uint32_t timer_get_ticks(void)
{
    return timer_ticks;
}
