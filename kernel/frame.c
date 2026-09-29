#include "frame.h"
#include "memory.h"
#include "terminal.h"
#include "../hindsight/event.h"

#define BOOTINFO_ADDRESS 0x7000
#define FRAME_BITMAP_ADDRESS 0x20000

struct boot_info
{
    uint32_t memory_entries;
    uint32_t memory_map;
} __attribute__((packed));

static struct boot_info* const boot_info =
    (struct boot_info*)BOOTINFO_ADDRESS;

static uint8_t* const frame_bitmap =
    (uint8_t*)FRAME_BITMAP_ADDRESS;

static uint32_t total_frames = 0;
static uint32_t free_frames = 0;
static uint32_t used_frames = 0;

static void bitmap_set(uint32_t frame)
{
    frame_bitmap[frame / 8] |=
        (uint8_t)(1 << (frame % 8));
}

static void bitmap_clear(uint32_t frame)
{
    frame_bitmap[frame / 8] &=
        (uint8_t)~(1 << (frame % 8));
}

static int bitmap_test(uint32_t frame)
{
    return
        (frame_bitmap[frame / 8] &
        (uint8_t)(1 << (frame % 8))) != 0;
}

static void reserve_all_frames(void)
{
    for (uint32_t i = 0; i < BITMAP_SIZE; i++)
    {
        frame_bitmap[i] = 0xFF;
    }

    total_frames = MAX_FRAMES;
    free_frames = 0;
    used_frames = MAX_FRAMES;
}

static void mark_usable_region(uint64_t base, uint64_t length)
{
    uint64_t end = base + length;

    /*
     * The current kernel is 32-bit protected mode.
     * Ignore memory above the 4 GiB address space.
     */
    if (base >= 0x100000000ULL)
    {
        return;
    }

    if (end > 0x100000000ULL)
    {
        end = 0x100000000ULL;
    }

    uint32_t first_frame =
        (uint32_t)((base + FRAME_SIZE - 1) / FRAME_SIZE);

    uint32_t last_frame =
        (uint32_t)(end / FRAME_SIZE);

    if (last_frame > MAX_FRAMES)
    {
        last_frame = MAX_FRAMES;
    }

    for (uint32_t frame = first_frame;
         frame < last_frame;
         frame++)
    {
        if (bitmap_test(frame))
        {
            bitmap_clear(frame);

            free_frames++;
            used_frames--;
        }
    }
}

void frame_init(void)
{
    struct e820_entry* entries =
        (struct e820_entry*)boot_info->memory_map;

    reserve_all_frames();

    for (uint32_t i = 0;
         i < boot_info->memory_entries;
         i++)
    {
        if (entries[i].type == E820_USABLE)
        {
            mark_usable_region(
                entries[i].base,
                entries[i].length
            );
        }
    }

    /*
     * Reserve the memory used by BlackForge itself.
     *
     * Bootloader / low memory
     */
    for (uint32_t address = 0x00000000;
         address < 0x00100000;
         address += FRAME_SIZE)
    {
        uint32_t frame = address / FRAME_SIZE;

        if (!bitmap_test(frame))
        {
            bitmap_set(frame);

            free_frames--;
            used_frames++;
        }
    }

    /*
     * Reserve the physical frame containing the bitmap.
     */
    for (uint32_t address = FRAME_BITMAP_ADDRESS;
         address < FRAME_BITMAP_ADDRESS + BITMAP_SIZE;
         address += FRAME_SIZE)
    {
        uint32_t frame = address / FRAME_SIZE;

        if (!bitmap_test(frame))
        {
            bitmap_set(frame);

            free_frames--;
            used_frames++;
        }
    }

    total_frames = MAX_FRAMES;
}

uint32_t frame_allocate(void)
{
    for (uint32_t frame = 0;
         frame < MAX_FRAMES;
         frame++)
    {
        if (!bitmap_test(frame))
        {
            bitmap_set(frame);

            free_frames--;
            used_frames++;

            uint32_t address = frame * FRAME_SIZE;

            /*
             * Hindsight:
             * Record every physical frame allocation.
             */
            hindsight_event_record(
                HINDSIGHT_EVENT_FRAME_ALLOC,
                0,
                address,
                FRAME_SIZE,
                0
            );

            return address;
        }
    }

    return 0;
}

void frame_free(uint32_t address)
{
    if (address % FRAME_SIZE != 0)
    {
        return;
    }

    uint32_t frame = address / FRAME_SIZE;

    if (frame >= MAX_FRAMES)
    {
        return;
    }

    if (bitmap_test(frame))
    {
        bitmap_clear(frame);

        free_frames++;
        used_frames--;

        /*
         * Hindsight:
         * Record every physical frame release.
         */
        hindsight_event_record(
            HINDSIGHT_EVENT_FRAME_FREE,
            0,
            address,
            FRAME_SIZE,
            0
        );
    }
}

uint32_t frame_get_total(void)
{
    return total_frames;
}

uint32_t frame_get_free(void)
{
    return free_frames;
}

uint32_t frame_get_used(void)
{
    return used_frames;
}

int frame_test(void)
{
    uint32_t initial_free = frame_get_free();

    uint32_t frame1 = frame_allocate();
    uint32_t frame2 = frame_allocate();
    uint32_t frame3 = frame_allocate();

    if (frame1 == 0 ||
        frame2 == 0 ||
        frame3 == 0)
    {
        if (frame1 != 0)
            frame_free(frame1);

        if (frame2 != 0)
            frame_free(frame2);

        if (frame3 != 0)
            frame_free(frame3);

        return 0;
    }

    if (frame1 == frame2 ||
        frame1 == frame3 ||
        frame2 == frame3)
    {
        frame_free(frame1);
        frame_free(frame2);
        frame_free(frame3);

        return 0;
    }

    if (frame_get_free() != initial_free - 3)
    {
        frame_free(frame1);
        frame_free(frame2);
        frame_free(frame3);

        return 0;
    }

    frame_free(frame1);
    frame_free(frame2);
    frame_free(frame3);

    if (frame_get_free() != initial_free)
    {
        return 0;
    }

    return 1;
}

void frame_reserve_range(uint32_t start, uint32_t end)
{
    if (end <= start)
    {
        return;
    }

    uint32_t first_frame =
        start / FRAME_SIZE;

    uint32_t last_frame =
        (end + FRAME_SIZE - 1) / FRAME_SIZE;

    if (last_frame > MAX_FRAMES)
    {
        last_frame = MAX_FRAMES;
    }

    for (uint32_t frame = first_frame;
         frame < last_frame;
         frame++)
    {
        if (!bitmap_test(frame))
        {
            bitmap_set(frame);

            free_frames--;
            used_frames++;
        }
    }
}
