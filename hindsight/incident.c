#include "incident.h"
#include "history.h"
#include "../kernel/terminal.h"

static int incident_initialized = 0;

void hindsight_incident_init(void)
{
    incident_initialized = 1;
}

int hindsight_incident_find_latest(HindsightIncident *incident)
{
    int i;
    unsigned int count;
    const HindsightEvent *event;

    if (!incident)
        return 0;

    incident->found = 0;
    incident->event_index = 0;

    if (!incident_initialized)
        hindsight_incident_init();

    count = hindsight_history_count();

    for (i = (int)count - 1; i >= 0; i--)
    {
        event = hindsight_history_get((unsigned int)i);

        if (!event)
            continue;

        if (event->severity == HINDSIGHT_ERROR ||
            event->severity == HINDSIGHT_CRITICAL)
        {
            incident->found = 1;
            incident->event_index = (unsigned int)i;
            incident->event = *event;

            return 1;
        }
    }

    return 0;
}

static void print_unsigned(unsigned long value)
{
    char buffer[16];
    int i = 0;

    if (value == 0)
    {
        terminal_putchar('0');
        return;
    }

    while (value > 0)
    {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
        terminal_putchar(buffer[--i]);
}

static void print_hex(unsigned long value)
{
    const char *hex = "0123456789ABCDEF";
    int i;

    terminal_write("0x");

    for (i = 7; i >= 0; i--)
    {
        terminal_putchar(
            hex[(value >> (i * 4)) & 0xF]
        );
    }
}

void hindsight_incident_print(const HindsightIncident *incident)
{
    if (!incident || !incident->found)
    {
        terminal_write("\nHINDSIGHT INCIDENT\n");
        terminal_write("------------------\n");
        terminal_write("No error or critical event found.\n\n");
        return;
    }

    terminal_write("\nHINDSIGHT INCIDENT\n");
    terminal_write("------------------\n");

    terminal_write("Event index : ");
    print_unsigned(incident->event_index);
    terminal_write("\n");

    terminal_write("Timestamp   : ");
    print_unsigned(incident->event.timestamp);
    terminal_write("\n");

terminal_write("Process     : ");

if (incident->event.process_id < 0)
{
    terminal_write("UNKNOWN");
}
else
{
    print_unsigned((unsigned long)incident->event.process_id);
}

terminal_write("\n");

    terminal_write("Address     : ");
    print_hex(incident->event.address);
    terminal_write("\n");

    terminal_write("Value       : ");
    print_hex(incident->event.value);
    terminal_write("\n");

    terminal_write("Description : ");
    terminal_write(incident->event.description);
    terminal_write("\n\n");
}
void hindsight_incident_print_context(
    const HindsightIncident *incident,
    unsigned int before
)
{
    unsigned int start;
    unsigned int i;
    const HindsightEvent *event;

    if (!incident || !incident->found)
        return;

    if (incident->event_index > before)
        start = incident->event_index - before;
    else
        start = 0;

    terminal_write("\nHINDSIGHT CONTEXT\n");
    terminal_write("-----------------\n");

    for (i = start;
         i <= incident->event_index;
         i++)
    {
        event = hindsight_history_get(i);

        if (!event)
            continue;

        terminal_write("[");
        print_unsigned(event->timestamp);
        terminal_write("] ");

        terminal_write(event->description);

        terminal_write("\n");
    }

    terminal_write("\n");
}
