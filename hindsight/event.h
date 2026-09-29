#ifndef HINDSIGHT_EVENT_H
#define HINDSIGHT_EVENT_H

#include <stdint.h>

#define HINDSIGHT_EVENT_CAPACITY 256

typedef enum
{
    HINDSIGHT_EVENT_NONE = 0,
    HINDSIGHT_EVENT_HEAP_ALLOC,
    HINDSIGHT_EVENT_HEAP_FREE,
    HINDSIGHT_EVENT_FRAME_ALLOC,
    HINDSIGHT_EVENT_FRAME_FREE,
    HINDSIGHT_EVENT_TASK_CREATE,
    HINDSIGHT_EVENT_TASK_EXIT,
    HINDSIGHT_EVENT_TASK_SWITCH,
    HINDSIGHT_EVENT_PAGE_FAULT
} hindsight_event_type_t;

typedef struct
{
    uint32_t sequence;
    uint32_t type;
    uint32_t timestamp;
    uint32_t pid;
    uint32_t address;
    uint32_t size;
    uint32_t extra;
} hindsight_event_t;

void hindsight_event_init(void);

void hindsight_event_record(
    hindsight_event_type_t type,
    uint32_t pid,
    uint32_t address,
    uint32_t size,
    uint32_t extra
);

const hindsight_event_t* hindsight_event_get(uint32_t index);

uint32_t hindsight_event_count(void);

#endif
