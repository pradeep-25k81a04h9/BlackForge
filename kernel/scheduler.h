#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "task.h"

void scheduler_init(void);

task_t* scheduler_get_current(void);

task_t* scheduler_select_next(void);

void scheduler_clear_current(void);

int scheduler_has_ready_tasks(void);

/*
 * Move the currently running task into
 * the BLOCKED state.
 *
 * Returns the next READY task selected
 * by the scheduler, or 0 if none exists.
 */
task_t* scheduler_block_current(void);

/*
 * Wake a blocked task.
 *
 * Returns 1 if the task was successfully
 * moved from BLOCKED to READY.
 */
int scheduler_wake_task(uint32_t pid);

int scheduler_test(void);

#endif
