#include "paging.h"
#include "frame.h"
#include "terminal.h"

#define PAGE_PRESENT   0x001
#define PAGE_WRITABLE  0x002
#define PAGE_ACCESSED  0x020

static uint32_t* const page_directory =
    (uint32_t*)PAGE_DIRECTORY_ADDRESS;


/*
 * Invalidate one virtual page from the CPU's TLB.
 */
static void invalidate_page(uint32_t virtual_address)
{
    __asm__ volatile (
        "invlpg (%0)"
        :
        : "r"(virtual_address)
        : "memory"
    );
}


/*
 * Get the page table belonging to a virtual address.
 *
 * We currently have page tables for only the
 * first 16 MiB of virtual memory.
 */
static uint32_t* get_page_table(uint32_t virtual_address)
{
    uint32_t directory_index =
        virtual_address >> 22;

    if (directory_index >= PAGING_TABLE_COUNT)
    {
        return 0;
    }

    uint32_t directory_entry =
        page_directory[directory_index];

    if ((directory_entry & PAGE_PRESENT) == 0)
    {
        return 0;
    }

    uint32_t page_table_address =
        directory_entry & 0xFFFFF000;

    return (uint32_t*)page_table_address;
}


static void clear_page_directory(void)
{
    for (uint32_t i = 0; i < 1024; i++)
    {
        page_directory[i] = 0;
    }
}


static void build_identity_page_tables(void)
{
    for (uint32_t table = 0;
         table < PAGING_TABLE_COUNT;
         table++)
    {
        uint32_t* page_table =
            (uint32_t*)(
                PAGE_TABLES_ADDRESS +
                table * PAGE_SIZE
            );

        for (uint32_t page = 0;
             page < 1024;
             page++)
        {
            uint32_t physical_address =
                (table * 1024 + page) * PAGE_SIZE;

            page_table[page] =
                physical_address |
                PAGE_PRESENT |
                PAGE_WRITABLE;
        }

        page_directory[table] =
            (uint32_t)page_table |
            PAGE_PRESENT |
            PAGE_WRITABLE;
    }
}


static void enable_paging(void)
{
    uint32_t page_directory_address =
        PAGE_DIRECTORY_ADDRESS;

    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"(page_directory_address)
        : "memory"
    );

    uint32_t cr0;

    __asm__ volatile (
        "mov %%cr0, %0"
        : "=r"(cr0)
    );

    cr0 |= 0x80000000;

    __asm__ volatile (
        "mov %0, %%cr0"
        :
        : "r"(cr0)
        : "memory"
    );
}


void paging_init(void)
{
    terminal_write("Paging        : INITIALIZING...\n");

    clear_page_directory();

    build_identity_page_tables();

    /*
     * Reserve the physical memory occupied by
     * the page directory and page tables.
     */
    frame_reserve_range(
        PAGE_DIRECTORY_ADDRESS,
        PAGE_TABLES_ADDRESS +
        PAGING_TABLE_COUNT * PAGE_SIZE
    );

    enable_paging();

    terminal_write("Paging        : ONLINE\n");
}


/*
 * Map one virtual page to one physical page.
 *
 * Both addresses must be 4 KiB aligned.
 *
 * Current implementation supports the first
 * 16 MiB of virtual memory.
 */
int map_page(uint32_t virtual_address,
             uint32_t physical_address)
{
    if ((virtual_address % PAGE_SIZE) != 0)
    {
        return 0;
    }

    if ((physical_address % PAGE_SIZE) != 0)
    {
        return 0;
    }

    if (virtual_address >= PAGING_MEMORY_SIZE)
    {
        return 0;
    }

    uint32_t* page_table =
        get_page_table(virtual_address);

    if (page_table == 0)
    {
        return 0;
    }

    uint32_t page_index =
        (virtual_address >> 12) & 0x3FF;

    page_table[page_index] =
        physical_address |
        PAGE_PRESENT |
        PAGE_WRITABLE;

    invalidate_page(virtual_address);

    return 1;
}


/*
 * Remove the mapping for one virtual page.
 */
int unmap_page(uint32_t virtual_address)
{
    if ((virtual_address % PAGE_SIZE) != 0)
    {
        return 0;
    }

    if (virtual_address >= PAGING_MEMORY_SIZE)
    {
        return 0;
    }

    uint32_t* page_table =
        get_page_table(virtual_address);

    if (page_table == 0)
    {
        return 0;
    }

    uint32_t page_index =
        (virtual_address >> 12) & 0x3FF;

    page_table[page_index] = 0;

    invalidate_page(virtual_address);

    return 1;
}


/*
 * Verify the paging structures and mappings.
 *
 * The CPU can automatically set the Accessed bit,
 * so that bit is ignored during comparisons.
 */
int paging_test(void)
{
    const uint32_t ENTRY_COMPARE_MASK =
        0xFFFFFFFF & ~PAGE_ACCESSED;


    /*
     * ==========================================
     * Page Directory Entry 0
     * ==========================================
     */

    uint32_t expected_directory_entry =
        PAGE_TABLES_ADDRESS |
        PAGE_PRESENT |
        PAGE_WRITABLE;

    uint32_t actual_directory_entry =
        page_directory[0];

    if ((actual_directory_entry & ENTRY_COMPARE_MASK) !=
        (expected_directory_entry & ENTRY_COMPARE_MASK))
    {
        return 0;
    }


    /*
     * ==========================================
     * Page Directory Entry 3
     * ==========================================
     */

    uint32_t expected_directory_entry_3 =
        (PAGE_TABLES_ADDRESS +
         (3 * PAGE_SIZE)) |
        PAGE_PRESENT |
        PAGE_WRITABLE;

    uint32_t actual_directory_entry_3 =
        page_directory[3];

    if ((actual_directory_entry_3 & ENTRY_COMPARE_MASK) !=
        (expected_directory_entry_3 & ENTRY_COMPARE_MASK))
    {
        return 0;
    }


    /*
     * ==========================================
     * First page table entry
     * ==========================================
     */

    uint32_t* first_page_table =
        (uint32_t*)PAGE_TABLES_ADDRESS;

    uint32_t expected_page_entry =
        PAGE_PRESENT |
        PAGE_WRITABLE;

    uint32_t actual_page_entry =
        first_page_table[0];

    if ((actual_page_entry & ENTRY_COMPARE_MASK) !=
        (expected_page_entry & ENTRY_COMPARE_MASK))
    {
        return 0;
    }


    /*
     * ==========================================
     * Identity mapping at 1 MiB
     * ==========================================
     */

    uint32_t page_index =
        0x00100000 / PAGE_SIZE;

    uint32_t expected_page_entry_1mb =
        0x00100000 |
        PAGE_PRESENT |
        PAGE_WRITABLE;

    uint32_t actual_page_entry_1mb =
        first_page_table[page_index];

    if ((actual_page_entry_1mb & ENTRY_COMPARE_MASK) !=
        (expected_page_entry_1mb & ENTRY_COMPARE_MASK))
    {
        return 0;
    }


    /*
     * ==========================================
     * Fourth page table
     * ==========================================
     */

    uint32_t* fourth_page_table =
        (uint32_t*)(
            PAGE_TABLES_ADDRESS +
            3 * PAGE_SIZE
        );

    uint32_t expected_fourth_entry =
        0x00C00000 |
        PAGE_PRESENT |
        PAGE_WRITABLE;

    uint32_t actual_fourth_entry =
        fourth_page_table[0];

    if ((actual_fourth_entry & ENTRY_COMPARE_MASK) !=
        (expected_fourth_entry & ENTRY_COMPARE_MASK))
    {
        return 0;
    }


    return 1;
}
