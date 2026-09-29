#include "event.h"

static hindsight_event_t events[HINDSIGHT_EVENT_CAPACITY];

static uint32_t event_write_index = 0;
static uint32_t event_count_value = 0;
static uint32_t event_sequence = 0;

void hindsight_event_init(void)
{
    event_write_index = 0;
    event_count_value = 0;
    event_sequence = 0;
}

void hindsight_event_record(
    hindsight_event_type_t type,
    uint32_t pid,
    uint32_t address,
    uint32_t size,
    uint32_t extra
)
{
    hindsight_event_t* event =
        &events[event_write_index];

    event->sequence = event_sequence++;
    event->type = (uint32_t)type;
    event->timestamp = 0;
    event->pid = pid;
    event->address = address;
    event->size = size;
    event->extra = extra;

    event_write_index =
        (event_write_index + 1) %
        HINDSIGHT_EVENT_CAPACITY;

    if (event_count_value <
        HINDSIGHT_EVENT_CAPACITY)
    {
        event_count_value++;
    }
}

const hindsight_event_t*
hindsight_event_get(uint32_t index)
{
    if (index >= event_count_value)
    {
        return 0;
    }

    /*
     * Convert the logical history index into
     * the physical ring-buffer index.
     *
     * When the buffer has not wrapped yet,
     * the oldest event is at index 0.
     *
     * After wrapping, event_write_index points
     * to the oldest retained event.
     */
    uint32_t physical_index;

    if (event_count_value <
        HINDSIGHT_EVENT_CAPACITY)
    {
        physical_index = index;
    }
    else
    {
        physical_index =
            (event_write_index + index) %
            HINDSIGHT_EVENT_CAPACITY;
    }

    return &events[physical_index];
}

uint32_t hindsight_event_count(void)
{
    return event_count_value;
}
