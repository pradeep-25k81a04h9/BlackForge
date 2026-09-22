#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

#define E820_USABLE 1

struct e820_entry
{
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
} __attribute__((packed));

void memory_init(void);
void memory_print_info(void);

#endif
