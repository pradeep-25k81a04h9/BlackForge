#include "history.h"

void hindsight_history_init(void)
{
    hindsight_event_init();
}

uint32_t hindsight_history_count(void)
{
    return hindsight_event_count();
}

const hindsight_event_t*
hindsight_history_get(uint32_t index)
{
    return hindsight_event_get(index);
}

void hindsight_history_clear(void)
{
    hindsight_event_init();
}
