#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

#define KERNEL_HEAP_START 0x00E00000
#define KERNEL_HEAP_END   0x00F00000

#define KERNEL_HEAP_PAGES \
    ((KERNEL_HEAP_END - KERNEL_HEAP_START) / 4096)

#define HEAP_MAX_ALLOCATIONS 128

void heap_init(void);

void* kmalloc(uint32_t size);
void kfree(void* address);

uint32_t heap_get_used_pages(void);
uint32_t heap_get_free_pages(void);

int heap_test(void);

#endif
