#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>

#define FRAME_SIZE 4096
#define MAX_MEMORY_32BIT 0xFFFFFFFF
#define MAX_FRAMES 1048576
#define BITMAP_SIZE (MAX_FRAMES / 8)

void frame_init(void);

uint32_t frame_allocate(void);
void frame_free(uint32_t address);

uint32_t frame_get_total(void);
uint32_t frame_get_free(void);
uint32_t frame_get_used(void);

int frame_test(void);

void frame_reserve_range(uint32_t start, uint32_t end);
#endif
