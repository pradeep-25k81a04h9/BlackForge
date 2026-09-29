#include "incident.h"
#include "history.h"

uint32_t hindsight_incident_count(uint32_t pid)
{
    uint32_t count = hindsight_history_count();
    uint32_t matches = 0;

    for (uint32_t i = 0; i < count; i++)
    {
        const hindsight_event_t* event =
            hindsight_history_get(i);

        if (event == 0)
        {
            continue;
        }

        if (event->pid == pid)
        {
            matches++;
        }
    }

    return matches;
}

const hindsight_event_t*
hindsight_incident_get(uint32_t pid, uint32_t index)
{
    uint32_t count = hindsight_history_count();
    uint32_t matches = 0;

    for (uint32_t i = 0; i < count; i++)
    {
        const hindsight_event_t* event =
            hindsight_history_get(i);

        if (event == 0)
        {
            continue;
        }

        if (event->pid == pid)
        {
            if (matches == index)
            {
                return event;
            }

            matches++;
        }
    }

    return 0;
}
