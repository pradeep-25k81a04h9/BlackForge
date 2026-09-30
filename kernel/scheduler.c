#include "scheduler.h"
#include "../hindsight/event.h"

static task_t* current_task = 0;

static uint32_t current_index = 0;


/*
 * Initialize the scheduler.
 */
void scheduler_init(void)
{
    current_task = 0;
    current_index = TASK_MAX - 1;
}

/*
 * Return the task currently selected
 * by the scheduler.
 */
task_t* scheduler_get_current(void)
{
    return current_task;
}
void scheduler_clear_current(void)
{
    current_task = 0;
}
int scheduler_has_ready_tasks(void)
{
    for (uint32_t i = 0; i < TASK_MAX; i++)
    {
        task_t* task =
            task_get_at(i);

        if (task != 0 &&
            task->state == TASK_READY)
        {
            return 1;
        }
    }

    return 0;
}
/*
 * Block the currently running task and
 * select another READY task.
 */
task_t* scheduler_block_current(void)
{
    if (current_task == 0)
        return 0;

    /*
     * The current task is no longer eligible
     * for scheduling.
     */
    current_task->state = TASK_BLOCKED;

    /*
     * scheduler_select_next() will skip the
     * blocked task and find the next READY task.
     */
    return scheduler_select_next();
}


/*
 * Wake a blocked task.
 */
int scheduler_wake_task(uint32_t pid)
{
    task_t* task = task_get(pid);

    if (task == 0)
        return 0;

    if (task->state != TASK_BLOCKED)
        return 0;

    task->state = TASK_READY;

    return 1;
}

/*
 * Select the next READY task using
 * round-robin scheduling.
 *
 * This function does NOT switch CPU
 * registers yet.
 *
 * It only decides which task should
 * run next.
 */
task_t* scheduler_select_next(void)
{
    for (uint32_t offset = 1;
         offset <= TASK_MAX;
         offset++)
    {
        uint32_t index =
            (current_index + offset) %
            TASK_MAX;

        task_t* task =
            task_get_at(index);

        if (task != 0 &&
            task->state == TASK_READY)
        {
            /*
             * Previous running task becomes
             * ready again.
             */
            if (current_task != 0 &&
                current_task->state ==
                    TASK_RUNNING)
            {
                current_task->state =
                    TASK_READY;
            }

current_task = task;

current_index = index;

current_task->state =
    TASK_RUNNING;

hindsight_record_event(
    HINDSIGHT_INTERRUPT,
    HINDSIGHT_INFO,
    (int)current_task->pid,
    0,
    0,
    "Task switch"
);
            return current_task;
        }
    }


    /*
     * No READY task exists.
     */
    return 0;
}


/*
 * Test round-robin task selection.
 */
static void scheduler_test_entry_a(void)
{
}


static void scheduler_test_entry_b(void)
{
}


static void scheduler_test_entry_c(void)
{
}


int scheduler_test(void)
{
    uint32_t initial_count =
        task_get_count();


    /*
     * Create three test tasks.
     */
    int pid_a =
        task_create(
            scheduler_test_entry_a
        );

    if (pid_a <= 0)
        return 0;


    int pid_b =
        task_create(
            scheduler_test_entry_b
        );

    if (pid_b <= 0)
    {
        task_destroy((uint32_t)pid_a);
        return 0;
    }


    int pid_c =
        task_create(
            scheduler_test_entry_c
        );

    if (pid_c <= 0)
    {
        task_destroy((uint32_t)pid_a);
        task_destroy((uint32_t)pid_b);
        return 0;
    }


    /*
     * Start from no current task.
     */
    current_task = 0;
    current_index = 0;


    /*
     * First selection.
     */
    task_t* first =
        scheduler_select_next();

    if (first == 0)
        goto fail;


    /*
     * Second selection.
     */
    task_t* second =
        scheduler_select_next();

    if (second == 0)
        goto fail;


    /*
     * Third selection.
     */
    task_t* third =
        scheduler_select_next();

    if (third == 0)
        goto fail;


    /*
     * We should have three different tasks.
     */
    if (first == second ||
        first == third ||
        second == third)
    {
        goto fail;
    }


    /*
     * Fourth selection should wrap around
     * to the first task.
     */
    task_t* fourth =
        scheduler_select_next();

    if (fourth != first)
        goto fail;


    /*
     * Only one task should be RUNNING.
     */
    if (first->state != TASK_RUNNING)
        goto fail;

    if (second->state != TASK_READY)
        goto fail;

    if (third->state != TASK_READY)
        goto fail;


    /*
     * Verify task count was restored later.
     */
    if (task_get_count() !=
        initial_count + 3)
    {
        goto fail;
    }


/*
 * Cleanup.
 *
 * The scheduler test deliberately leaves one task
 * selected as RUNNING. Clear the scheduler's current
 * task before destroying the temporary test tasks.
 */
current_task = 0;

task_destroy((uint32_t)pid_a);
task_destroy((uint32_t)pid_b);
task_destroy((uint32_t)pid_c);

current_index = 0;

    if (task_get_count() !=
        initial_count)
    {
        return 0;
    }


    return 1;


fail:

current_task = 0;

task_destroy((uint32_t)pid_a);
task_destroy((uint32_t)pid_b);
task_destroy((uint32_t)pid_c);

current_index = 0;

return 0;
}
