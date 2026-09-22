#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "task.h"

void scheduler_init(void);

task_t* scheduler_get_current(void);

task_t* scheduler_select_next(void);

int scheduler_test(void);

void scheduler_clear_current(void);

#endif
