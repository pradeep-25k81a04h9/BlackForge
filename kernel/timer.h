#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(void);

uint32_t timer_interrupt_handler(
    uint32_t* stack_pointer
);

uint32_t timer_get_ticks(void);

#endif
