#include "heap.h"
#include "paging.h"
#include "frame.h"
#include "terminal.h"
#include "../hindsight/event.h"

#define HEAP_PAGE_SIZE 4096

/*
 * Allocations up to this size use the block allocator.
 *
 * Larger allocations use whole pages.
 */
#define SMALL_ALLOCATION_LIMIT 2048

/*
 * Every heap page begins with a small management
 * structure.
 */
struct heap_page
{
    uint32_t initialized;
};

/*
 * A block describes one allocation/free region
 * inside a heap page.
 *
 * For large allocations this structure is placed
 * at the beginning of the first allocated page.
 */
struct heap_block
{
    uint32_t size;
    uint32_t used;
};

/*
 * Physical frame backing each virtual heap page.
 *
 * 0 means the page is currently unmapped.
 */
static uint32_t heap_page_frames[KERNEL_HEAP_PAGES];

/*
 * Number of heap pages currently mapped.
 */
static uint32_t mapped_pages = 0;

/*
 * Number of pages currently occupied by allocations.
 */
static uint32_t used_pages = 0;


/*
 * Align a size upward to 8 bytes.
 */
static uint32_t align_up_8(uint32_t value)
{
    return (value + 7) & ~7;
}


/*
 * Convert a heap page number to its virtual address.
 */
static uint32_t heap_page_address(uint32_t page)
{
    return KERNEL_HEAP_START +
           page * HEAP_PAGE_SIZE;
}


/*
 * Map one previously unused heap page.
 */
static int map_heap_page(uint32_t page)
{
    if (page >= KERNEL_HEAP_PAGES)
        return 0;

    if (heap_page_frames[page] != 0)
        return 1;

    uint32_t physical =
        frame_allocate();

    if (physical == 0)
        return 0;

    uint32_t virtual_address =
        heap_page_address(page);

    if (!map_page(
            virtual_address,
            physical))
    {
        frame_free(physical);
        return 0;
    }

    heap_page_frames[page] = physical;

    mapped_pages++;

    return 1;
}


/*
 * Unmap a heap page and return its physical
 * frame to the frame allocator.
 */
static void unmap_heap_page(uint32_t page)
{
    if (page >= KERNEL_HEAP_PAGES)
        return;

    uint32_t physical =
        heap_page_frames[page];

    if (physical == 0)
        return;

    unmap_page(
        heap_page_address(page)
    );

    frame_free(physical);

    heap_page_frames[page] = 0;

    if (mapped_pages > 0)
        mapped_pages--;
}


/*
 * Initialize one heap page as a single large
 * free block.
 *
 * Layout:
 *
 * +-------------------------+
 * | heap_page               |
 * +-------------------------+
 * | heap_block              |
 * +-------------------------+
 * |                         |
 * |      free memory        |
 * |                         |
 * +-------------------------+
 */
static void initialize_heap_page(uint32_t page)
{
    uint8_t* page_memory =
        (uint8_t*)heap_page_address(page);

    struct heap_page* header =
        (struct heap_page*)page_memory;

    header->initialized = 1;

    struct heap_block* block =
        (struct heap_block*)(
            page_memory +
            sizeof(struct heap_page)
        );

    block->size =
        HEAP_PAGE_SIZE -
        sizeof(struct heap_page) -
        sizeof(struct heap_block);

    block->used = 0;
}


/*
 * Find a free block inside a heap page.
 */
static struct heap_block*
find_free_block(
    uint32_t page,
    uint32_t required_size)
{
    uint8_t* page_memory =
        (uint8_t*)heap_page_address(page);

    uint32_t offset =
        sizeof(struct heap_page);

    uint32_t page_end =
        HEAP_PAGE_SIZE;

    while (offset +
           sizeof(struct heap_block) <=
           page_end)
    {
        struct heap_block* block =
            (struct heap_block*)(
                page_memory + offset
            );

        if (block->size == 0)
            return 0;

        if (!block->used &&
            block->size >= required_size)
        {
            return block;
        }

        uint32_t next =
            sizeof(struct heap_block) +
            block->size;

        if (next == 0)
            return 0;

        offset += next;

        if (offset > page_end)
            return 0;
    }

    return 0;
}


/*
 * Split a free block if enough space remains
 * for another block.
 */
static void split_block(
    struct heap_block* block,
    uint32_t requested_size)
{
    uint32_t minimum_remaining =
        sizeof(struct heap_block) + 8;

    if (block->size <=
        requested_size + minimum_remaining)
    {
        return;
    }

    uint8_t* block_memory =
        (uint8_t*)block;

    struct heap_block* new_block =
        (struct heap_block*)(
            block_memory +
            sizeof(struct heap_block) +
            requested_size
        );

    new_block->size =
        block->size -
        requested_size -
        sizeof(struct heap_block);

    new_block->used = 0;

    block->size = requested_size;
}


/*
 * Find a free block throughout mapped heap pages.
 */
static struct heap_block*
find_block(
    uint32_t required_size,
    uint32_t* result_page)
{
    for (uint32_t page = 0;
         page < KERNEL_HEAP_PAGES;
         page++)
    {
        if (heap_page_frames[page] == 0)
            continue;

        struct heap_block* block =
            find_free_block(
                page,
                required_size
            );

        if (block != 0)
        {
            if (result_page != 0)
                *result_page = page;

            return block;
        }
    }

    return 0;
}


/*
 * Check whether a heap page contains any
 * allocated small blocks.
 */
static int page_has_used_blocks(uint32_t page)
{
    uint8_t* page_memory =
        (uint8_t*)heap_page_address(page);

    uint32_t offset =
        sizeof(struct heap_page);

    while (offset +
           sizeof(struct heap_block) <=
           HEAP_PAGE_SIZE)
    {
        struct heap_block* block =
            (struct heap_block*)(
                page_memory + offset
            );

        if (block->size == 0)
            break;

        if (block->used)
            return 1;

        offset +=
            sizeof(struct heap_block) +
            block->size;
    }

    return 0;
}


/*
 * Coalesce adjacent free blocks inside one page.
 */
static void merge_free_blocks(uint32_t page)
{
    uint8_t* page_memory =
        (uint8_t*)heap_page_address(page);

    uint32_t offset =
        sizeof(struct heap_page);

    while (offset +
           sizeof(struct heap_block) <=
           HEAP_PAGE_SIZE)
    {
        struct heap_block* first =
            (struct heap_block*)(
                page_memory + offset
            );

        if (first->size == 0)
            break;

        uint32_t next_offset =
            offset +
            sizeof(struct heap_block) +
            first->size;

        if (next_offset +
            sizeof(struct heap_block) >
            HEAP_PAGE_SIZE)
        {
            break;
        }

        struct heap_block* second =
            (struct heap_block*)(
                page_memory + next_offset
            );

        if (second->size == 0)
            break;

        if (!first->used &&
            !second->used)
        {
            first->size +=
                sizeof(struct heap_block) +
                second->size;

            continue;
        }

        offset = next_offset;
    }
}


/*
 * Initialize the kernel heap.
 */
void heap_init(void)
{
    terminal_write(
        "Heap          : INITIALIZING...\n"
    );

    /*
     * The heap occupies:
     *
     * 0x00E00000 - 0x00EFFFFF
     *
     * Remove the identity mappings.
     */
    for (uint32_t page = 0;
         page < KERNEL_HEAP_PAGES;
         page++)
    {
        heap_page_frames[page] = 0;

        unmap_page(
            heap_page_address(page)
        );
    }

    mapped_pages = 0;
    used_pages = 0;

    terminal_write(
        "Heap          : ONLINE\n"
    );
}


/*
 * Allocate memory from the kernel heap.
 */
void* kmalloc(uint32_t size)
{
    if (size == 0)
        return 0;


    /*
     * ==========================================
     * SMALL ALLOCATION
     * ==========================================
     */
    if (size <= SMALL_ALLOCATION_LIMIT)
    {
        uint32_t required_size =
            align_up_8(size);

        uint32_t page;

        struct heap_block* block =
            find_block(
                required_size,
                &page
            );

        /*
         * No existing free block.
         *
         * Find an unused heap page.
         */
        if (block == 0)
        {
            page = KERNEL_HEAP_PAGES;

            for (uint32_t i = 0;
                 i < KERNEL_HEAP_PAGES;
                 i++)
            {
                if (heap_page_frames[i] == 0)
                {
                    page = i;
                    break;
                }
            }

            if (page >= KERNEL_HEAP_PAGES)
                return 0;

            if (!map_heap_page(page))
                return 0;

            initialize_heap_page(page);

            block =
                find_free_block(
                    page,
                    required_size
                );

            if (block == 0)
            {
                unmap_heap_page(page);
                return 0;
            }

            used_pages++;
        }

        split_block(
            block,
            required_size
        );

block->used = 1;

void *address =
    (void*)(
        (uint8_t*)block +
        sizeof(struct heap_block)
    );

hindsight_record_event(
    HINDSIGHT_MEMORY_ALLOC,
    HINDSIGHT_INFO,
    -1,
    (unsigned long)address,
    size,
    "Heap allocation"
);

return address;
    }


    /*
     * ==========================================
     * LARGE ALLOCATION
     * ==========================================
     *
     * Allocations above 2048 bytes are backed
     * directly by consecutive heap pages.
     */
    uint32_t total_size =
        size +
        sizeof(struct heap_block);

    uint32_t page_count =
        (total_size + HEAP_PAGE_SIZE - 1) /
        HEAP_PAGE_SIZE;

    if (page_count == 0 ||
        page_count > KERNEL_HEAP_PAGES)
    {
        return 0;
    }

    /*
     * Find consecutive unused pages.
     */
    uint32_t start_page =
        KERNEL_HEAP_PAGES;

    for (uint32_t start = 0;
         start <= KERNEL_HEAP_PAGES -
                   page_count;
         start++)
    {
        int free = 1;

        for (uint32_t page = 0;
             page < page_count;
             page++)
        {
            if (heap_page_frames[
                    start + page] != 0)
            {
                free = 0;
                break;
            }
        }

        if (free)
        {
            start_page = start;
            break;
        }
    }

    if (start_page >= KERNEL_HEAP_PAGES)
        return 0;

    /*
     * Map all required pages.
     */
    uint32_t mapped = 0;

    for (uint32_t page = 0;
         page < page_count;
         page++)
    {
        if (!map_heap_page(
                start_page + page))
        {
            for (uint32_t rollback = 0;
                 rollback < mapped;
                 rollback++)
            {
                unmap_heap_page(
                    start_page + rollback
                );
            }

            return 0;
        }

        mapped++;
    }

    /*
     * Store allocation metadata at the
     * beginning of the first page.
     */
    struct heap_block* block =
        (struct heap_block*)(
            heap_page_address(start_page)
        );

    block->size = size;
    block->used = 1;

used_pages += page_count;

void *address =
    (void*)(
        (uint8_t*)block +
        sizeof(struct heap_block)
    );

hindsight_record_event(
    HINDSIGHT_MEMORY_ALLOC,
    HINDSIGHT_INFO,
    -1,
    (unsigned long)address,
    size,
    "Heap allocation"
);

return address;
}


/*
 * Free a kernel heap allocation.
 */
void kfree(void* address)
{
    if (address == 0)
        return;

    uint32_t virtual_address =
        (uint32_t)address;

    if (virtual_address <
            KERNEL_HEAP_START ||
        virtual_address >=
            KERNEL_HEAP_END)
    {
        return;
    }

    uint32_t page =
        (virtual_address -
         KERNEL_HEAP_START) /
        HEAP_PAGE_SIZE;

    if (page >= KERNEL_HEAP_PAGES)
        return;

    if (heap_page_frames[page] == 0)
        return;


    /*
     * ==========================================
     * LARGE ALLOCATION
     * ==========================================
     *
     * IMPORTANT:
     *
     * A large allocation begins with its
     * heap_block at the very beginning of
     * the page.
     *
     * Therefore the returned pointer is:
     *
     *     page_address + sizeof(heap_block)
     *
     * This check MUST happen before the small
     * allocation logic.
     *
     * Otherwise a 4096-byte allocation can
     * incorrectly be treated as a small block
     * because 4096 <= HEAP_PAGE_SIZE.
     */
    uint32_t page_address =
        heap_page_address(page);

    uint32_t offset =
        virtual_address -
        page_address;

    if (offset == sizeof(struct heap_block))
    {
        struct heap_block* block =
            (struct heap_block*)page_address;

        if (block->used &&
            block->size > SMALL_ALLOCATION_LIMIT)
        {
            uint32_t size =
                block->size;

            uint32_t total_size =
                size +
                sizeof(struct heap_block);

            uint32_t page_count =
                (total_size +
                 HEAP_PAGE_SIZE - 1) /
                HEAP_PAGE_SIZE;

            if (page_count == 0 ||
                page + page_count >
                    KERNEL_HEAP_PAGES)
            {
                return;
            }

hindsight_record_event(
    HINDSIGHT_MEMORY_FREE,
    HINDSIGHT_INFO,
    -1,
    (unsigned long)virtual_address,
    size,
    "Heap free"
);

block->used = 0;
            for (uint32_t i = 0;
                 i < page_count;
                 i++)
            {
                unmap_heap_page(page + i);
            }

            if (used_pages >= page_count)
                used_pages -= page_count;
            else
                used_pages = 0;

            return;
        }
    }


    /*
     * ==========================================
     * SMALL ALLOCATION
     * ==========================================
     */
    if (offset >=
        sizeof(struct heap_page) +
        sizeof(struct heap_block))
    {
        struct heap_block* block =
            (struct heap_block*)(
                virtual_address -
                sizeof(struct heap_block)
            );

        /*
         * Make sure this is actually a valid
         * small block belonging to this page.
         */
        uint8_t* page_memory =
            (uint8_t*)page_address;

        uint8_t* block_address =
            (uint8_t*)block;

        if (block_address <
            page_memory +
            sizeof(struct heap_page))
        {
            return;
        }

if (block->used &&
    block->size <= SMALL_ALLOCATION_LIMIT)
{
    uint32_t size = block->size;

    hindsight_record_event(
        HINDSIGHT_MEMORY_FREE,
        HINDSIGHT_INFO,
        -1,
        (unsigned long)virtual_address,
        size,
        "Heap free"
    );

    block->used = 0;

    merge_free_blocks(page);
            /*
             * If this page is completely free,
             * return its physical frame.
             */
            if (!page_has_used_blocks(page))
            {
                unmap_heap_page(page);

                if (used_pages > 0)
                    used_pages--;
            }

            return;
        }
    }
}


/*
 * Return number of pages currently used.
 */
uint32_t heap_get_used_pages(void)
{
    return used_pages;
}


/*
 * Return number of currently free heap pages.
 */
uint32_t heap_get_free_pages(void)
{
    return KERNEL_HEAP_PAGES - used_pages;
}


/*
 * ==========================================
 * HEAP TEST
 * ==========================================
 */
int heap_test(void)
{
    uint32_t initial_free =
        heap_get_free_pages();

    /*
     * Allocate several small blocks.
     */
    uint8_t* first =
        (uint8_t*)kmalloc(16);

    uint8_t* second =
        (uint8_t*)kmalloc(100);

    uint8_t* third =
        (uint8_t*)kmalloc(500);

    if (first == 0 ||
        second == 0 ||
        third == 0)
    {
        kfree(first);
        kfree(second);
        kfree(third);

        return 0;
    }

    /*
     * They should all live inside the
     * same heap page.
     */
    uint32_t first_page =
        ((uint32_t)first -
         KERNEL_HEAP_START) /
        HEAP_PAGE_SIZE;

    uint32_t second_page =
        ((uint32_t)second -
         KERNEL_HEAP_START) /
        HEAP_PAGE_SIZE;

    uint32_t third_page =
        ((uint32_t)third -
         KERNEL_HEAP_START) /
        HEAP_PAGE_SIZE;

    if (first_page != second_page ||
        first_page != third_page)
    {
        kfree(first);
        kfree(second);
        kfree(third);

        return 0;
    }

    /*
     * Test actual memory access.
     */
    first[0] = 0x11;
    second[0] = 0x22;
    third[0] = 0x33;

    if (first[0] != 0x11 ||
        second[0] != 0x22 ||
        third[0] != 0x33)
    {
        kfree(first);
        kfree(second);
        kfree(third);

        return 0;
    }

    /*
     * Free the middle block.
     */
    kfree(second);

    /*
     * Allocate another block.
     *
     * The allocator should reuse the free space.
     */
    uint8_t* reused =
        (uint8_t*)kmalloc(80);

    if (reused == 0)
    {
        kfree(first);
        kfree(third);

        return 0;
    }

    /*
     * Test reused block.
     */
    reused[0] = 0x44;

    if (reused[0] != 0x44)
    {
        kfree(first);
        kfree(third);
        kfree(reused);

        return 0;
    }

    /*
     * Large allocation.
     */
    uint32_t* large =
        (uint32_t*)kmalloc(7000);

    if (large == 0)
    {
        kfree(first);
        kfree(third);
        kfree(reused);

        return 0;
    }

    large[0] = 0x12345678;
    large[1000] = 0xAABBCCDD;

    if (large[0] != 0x12345678 ||
        large[1000] != 0xAABBCCDD)
    {
        kfree(first);
        kfree(third);
        kfree(reused);
        kfree(large);

        return 0;
    }

    /*
     * Free everything.
     */
    kfree(first);
    kfree(third);
    kfree(reused);
    kfree(large);

    /*
     * Heap should return to its original
     * state.
     */
    if (heap_get_free_pages() != initial_free)
        return 0;

    return 1;
}
