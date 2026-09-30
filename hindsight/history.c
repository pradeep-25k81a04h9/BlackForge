#include "history.h"
#include "../kernel/storage/storage.h"

#define HINDSIGHT_STORAGE_MAGIC   0x48495354UL
#define HINDSIGHT_STORAGE_VERSION 1
#define HINDSIGHT_STORAGE_LBA     1

#define HINDSIGHT_EVENT_DISK_SIZE 84
#define HINDSIGHT_EVENTS_PER_SECTOR 6
#define HINDSIGHT_EVENT_SECTORS 22

static HindsightEvent history[HINDSIGHT_HISTORY_SIZE];

static unsigned int history_write_index = 0;
static unsigned int history_count_value = 0;

static unsigned long history_last_timestamp_value = 0;

static void clear_buffer(unsigned char *buffer)
{
    unsigned int i;

    for (i = 0; i < STORAGE_SECTOR_SIZE; i++)
        buffer[i] = 0;
}

static void write_u32(unsigned char *buffer, unsigned int offset,
                      unsigned long value)
{
    buffer[offset + 0] = (unsigned char)(value & 0xFF);
    buffer[offset + 1] = (unsigned char)((value >> 8) & 0xFF);
    buffer[offset + 2] = (unsigned char)((value >> 16) & 0xFF);
    buffer[offset + 3] = (unsigned char)((value >> 24) & 0xFF);
}

static unsigned long read_u32(const unsigned char *buffer,
                              unsigned int offset)
{
    return
        ((unsigned long)buffer[offset + 0]) |
        ((unsigned long)buffer[offset + 1] << 8) |
        ((unsigned long)buffer[offset + 2] << 16) |
        ((unsigned long)buffer[offset + 3] << 24);
}

static void serialize_event(unsigned char *buffer,
                            unsigned int offset,
                            const HindsightEvent *event)
{
    unsigned int i;

    write_u32(buffer, offset + 0, event->timestamp);
    write_u32(buffer, offset + 4, event->type);
    write_u32(buffer, offset + 8, event->severity);
    write_u32(buffer, offset + 12, (unsigned long)event->process_id);
    write_u32(buffer, offset + 16, event->address);
    write_u32(buffer, offset + 20, event->value);

    for (i = 0; i < HINDSIGHT_MAX_DESCRIPTION; i++)
        buffer[offset + 24 + i] =
            (unsigned char)event->description[i];
}

static void deserialize_event(const unsigned char *buffer,
                              unsigned int offset,
                              HindsightEvent *event)
{
    unsigned int i;

    event->timestamp = read_u32(buffer, offset + 0);
    event->type = (HindsightEventType)read_u32(buffer, offset + 4);
    event->severity = (HindsightSeverity)read_u32(buffer, offset + 8);
    event->process_id = (int)read_u32(buffer, offset + 12);
    event->address = read_u32(buffer, offset + 16);
    event->value = read_u32(buffer, offset + 20);

    for (i = 0; i < HINDSIGHT_MAX_DESCRIPTION; i++)
        event->description[i] =
            (char)buffer[offset + 24 + i];

    event->description[HINDSIGHT_MAX_DESCRIPTION - 1] = '\0';
}

void hindsight_history_init(void)
{
    history_write_index = 0;
    history_count_value = 0;
    history_last_timestamp_value = 0;
}

void hindsight_history_add(const HindsightEvent *event)
{
    history[history_write_index] = *event;

    history_write_index++;

    if (history_write_index >= HINDSIGHT_HISTORY_SIZE)
        history_write_index = 0;

    if (history_count_value < HINDSIGHT_HISTORY_SIZE)
        history_count_value++;

    if (event->timestamp > history_last_timestamp_value)
        history_last_timestamp_value = event->timestamp;
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

unsigned long hindsight_history_last_timestamp(void)
{
    return history_last_timestamp_value;
}

int hindsight_history_save(void)
{
    unsigned char buffer[STORAGE_SECTOR_SIZE];
    unsigned int i;
    unsigned int count;

    if (!storage_is_available())
        return -1;

    clear_buffer(buffer);

    write_u32(buffer, 0, HINDSIGHT_STORAGE_MAGIC);
    write_u32(buffer, 4, HINDSIGHT_STORAGE_VERSION);
    write_u32(buffer, 8, history_count_value);
    write_u32(buffer, 12, history_write_index);
    write_u32(buffer, 16, history_last_timestamp_value);

    if (storage_write_sector(HINDSIGHT_STORAGE_LBA, buffer) != 0)
        return -1;

    count = history_count_value;

    for (i = 0; i < HINDSIGHT_EVENT_SECTORS; i++)
    {
        unsigned char sector[STORAGE_SECTOR_SIZE];
        unsigned int slot;
        unsigned int j;

        clear_buffer(sector);

        for (j = 0; j < HINDSIGHT_EVENTS_PER_SECTOR; j++)
        {
            unsigned int event_index;
            unsigned int offset;
            const HindsightEvent *event;

            slot = i * HINDSIGHT_EVENTS_PER_SECTOR + j;

            if (slot >= count)
                break;

            event_index = slot;
            event = hindsight_history_get(event_index);

            if (!event)
                continue;

            offset = j * HINDSIGHT_EVENT_DISK_SIZE;
            serialize_event(sector, offset, event);
        }

        if (storage_write_sector(
                HINDSIGHT_STORAGE_LBA + 1 + i,
                sector) != 0)
            return -1;
    }

    return 0;
}

int hindsight_history_load(void)
{
    unsigned char buffer[STORAGE_SECTOR_SIZE];
    unsigned int count;
    unsigned int i;

    if (!storage_is_available())
        return -1;

    if (storage_read_sector(HINDSIGHT_STORAGE_LBA, buffer) != 0)
        return -1;

    if (read_u32(buffer, 0) != HINDSIGHT_STORAGE_MAGIC)
        return -1;

    if (read_u32(buffer, 4) != HINDSIGHT_STORAGE_VERSION)
        return -1;

    count = (unsigned int)read_u32(buffer, 8);

    if (count > HINDSIGHT_HISTORY_SIZE)
        return -1;

    history_count_value = 0;
    history_write_index = 0;
    history_last_timestamp_value =
        read_u32(buffer, 16);

    for (i = 0; i < count; i++)
    {
        unsigned int sector_index;
        unsigned int event_index;
        unsigned int offset;
        unsigned char sector[STORAGE_SECTOR_SIZE];
        HindsightEvent event;

        sector_index = i / HINDSIGHT_EVENTS_PER_SECTOR;
        event_index = i % HINDSIGHT_EVENTS_PER_SECTOR;
        offset = event_index * HINDSIGHT_EVENT_DISK_SIZE;

        if (storage_read_sector(
                HINDSIGHT_STORAGE_LBA + 1 + sector_index,
                sector) != 0)
            return -1;

        deserialize_event(sector, offset, &event);

        history[history_count_value] = event;
        history_count_value++;

        if (event.timestamp > history_last_timestamp_value)
            history_last_timestamp_value = event.timestamp;
    }

    history_write_index =
        history_count_value % HINDSIGHT_HISTORY_SIZE;

    return 0;
}
