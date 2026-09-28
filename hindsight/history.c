#include "history.h"

static HindsightEvent history[HINDSIGHT_HISTORY_SIZE];

static unsigned int history_write_index = 0;
static unsigned int history_count_value = 0;

void hindsight_history_init(void)
{
    history_write_index = 0;
    history_count_value = 0;
}

void hindsight_history_add(const HindsightEvent *event)
{
    history[history_write_index] = *event;

    history_write_index++;

    if (history_write_index >= HINDSIGHT_HISTORY_SIZE)
        history_write_index = 0;

    if (history_count_value < HINDSIGHT_HISTORY_SIZE)
        history_count_value++;
}

const HindsightEvent *hindsight_history_get(unsigned int index)
{
    unsigned int oldest_index;
    unsigned int actual_index;

    if (index >= history_count_value)
        return 0;

    if (history_count_value < HINDSIGHT_HISTORY_SIZE)
        oldest_index = 0;
    else
        oldest_index = history_write_index;

    actual_index = oldest_index + index;

    if (actual_index >= HINDSIGHT_HISTORY_SIZE)
        actual_index -= HINDSIGHT_HISTORY_SIZE;

    return &history[actual_index];
}

unsigned int hindsight_history_count(void)
{
    return history_count_value;
}

