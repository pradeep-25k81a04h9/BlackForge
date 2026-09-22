#include "memory.h"
#include "terminal.h"

#define BOOTINFO_ADDRESS 0x7000

struct boot_info
{
    uint32_t memory_entries;
    uint32_t memory_map;
} __attribute__((packed));

static struct boot_info* const boot_info =
    (struct boot_info*)BOOTINFO_ADDRESS;

static uint64_t usable_memory = 0;
static uint64_t reserved_memory = 0;

static void print_hex32(uint32_t value)
{
    const char* hex = "0123456789ABCDEF";

    for (int shift = 28; shift >= 0; shift -= 4)
    {
        terminal_putchar(hex[(value >> shift) & 0xF]);
    }
}

static void print_hex64(uint64_t value)
{
    uint32_t high = (uint32_t)(value >> 32);
    uint32_t low = (uint32_t)value;

    print_hex32(high);
    print_hex32(low);
}

static void print_u32(uint32_t value)
{
    char buffer[10];
    int position = 0;

    if (value == 0)
    {
        terminal_putchar('0');
        return;
    }

    while (value > 0)
    {
        buffer[position++] = '0' + (value % 10);
        value /= 10;
    }

    while (position > 0)
    {
        terminal_putchar(buffer[--position]);
    }
}

void memory_init(void)
{
    struct e820_entry* entries =
        (struct e820_entry*)boot_info->memory_map;

    usable_memory = 0;
    reserved_memory = 0;

    for (uint32_t i = 0;
         i < boot_info->memory_entries;
         i++)
    {
        if (entries[i].type == E820_USABLE)
        {
            usable_memory += entries[i].length;
        }
        else
        {
            reserved_memory += entries[i].length;
        }
    }
}

void memory_print_info(void)
{
    struct e820_entry* entries =
        (struct e820_entry*)boot_info->memory_map;

    terminal_write("\nBlackForge Memory Manager\n");
    terminal_write("-------------------------\n");

    terminal_write("E820 entries : ");
    print_u32(boot_info->memory_entries);
    terminal_write("\n");

    terminal_write("Usable RAM   : 0x");
    print_hex64(usable_memory);
    terminal_write(" bytes\n");

    terminal_write("Reserved RAM : 0x");
    print_hex64(reserved_memory);
    terminal_write(" bytes\n\n");

    terminal_write("E820 Memory Map\n");
    terminal_write("-------------------------\n");

    for (uint32_t i = 0;
         i < boot_info->memory_entries;
         i++)
    {
        terminal_write("Entry ");
        print_u32(i);
        terminal_write(": ");

        terminal_write("Base=0x");
        print_hex64(entries[i].base);

        terminal_write(" Length=0x");
        print_hex64(entries[i].length);

        terminal_write(" Type=");
        print_u32(entries[i].type);

        terminal_write("\n");
    }

    terminal_write("\n");
}
