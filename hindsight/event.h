#ifndef HINDSIGHT_EVENT_H
#define HINDSIGHT_EVENT_H

#define HINDSIGHT_MAX_DESCRIPTION 64

typedef enum {
    HINDSIGHT_INFO,
    HINDSIGHT_WARNING,
    HINDSIGHT_ERROR,
    HINDSIGHT_CRITICAL
} HindsightSeverity;

typedef enum {
    HINDSIGHT_PROCESS_START,
    HINDSIGHT_PROCESS_EXIT,
    HINDSIGHT_MEMORY_ALLOC,
    HINDSIGHT_MEMORY_FREE,
    HINDSIGHT_PAGE_FAULT,
    HINDSIGHT_INTERRUPT,
    HINDSIGHT_KERNEL_ERROR,
    HINDSIGHT_SYSTEM_CRASH
} HindsightEventType;

typedef struct {
    unsigned long timestamp;
    HindsightEventType type;
    HindsightSeverity severity;
    int process_id;
    unsigned long address;
    unsigned long value;
    char description[HINDSIGHT_MAX_DESCRIPTION];
} HindsightEvent;

void hindsight_record_event(
    HindsightEventType type,
    HindsightSeverity severity,
    int process_id,
    unsigned long address,
    unsigned long value,
    const char *description
);
void hindsight_set_timestamp(unsigned long timestamp);

#endif
