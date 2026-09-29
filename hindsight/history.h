#ifndef HINDSIGHT_HISTORY_H
#define HINDSIGHT_HISTORY_H

#include <stdint.h>
#include "event.h"

void hindsight_history_init(void);

uint32_t hindsight_history_count(void);

const hindsight_event_t*
hindsight_history_get(uint32_t index);

void hindsight_history_clear(void);

#endif
