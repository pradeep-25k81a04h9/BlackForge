#include "event.h"
#include "history.h"

static unsigned long hindsight_timestamp = 0;

void hindsight_record_event(
    HindsightEventType type,
    HindsightSeverity severity,
    int process_id,
    unsigned long address,
    unsigned long value,
    const char *description
) {
    HindsightEvent event;
    int i;

    hindsight_timestamp++;

    event.timestamp = hindsight_timestamp;
    event.type = type;
    event.severity = severity;
    event.process_id = process_id;
    event.address = address;
    event.value = value;

    for (i = 0; i < HINDSIGHT_MAX_DESCRIPTION - 1; i++) {
        if (description[i] == '\0')
            break;

        event.description[i] = description[i];
    }

    event.description[i] = '\0';

    hindsight_history_add(&event);
}
