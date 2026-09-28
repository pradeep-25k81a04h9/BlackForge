#ifndef HINDSIGHT_HISTORY_H
#define HINDSIGHT_HISTORY_H

#include "event.h"

#define HINDSIGHT_HISTORY_SIZE 128

void hindsight_history_init(void);

void hindsight_history_add(const HindsightEvent *event);

const HindsightEvent *hindsight_history_get(unsigned int index);

unsigned int hindsight_history_count(void);

#endif
