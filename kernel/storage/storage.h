#ifndef BLACKFORGE_STORAGE_H
#define BLACKFORGE_STORAGE_H

#include <stdint.h>

#define STORAGE_SECTOR_SIZE 512

int storage_init(void);

int storage_read_sector(
    uint32_t lba,
    uint8_t *buffer
);

int storage_write_sector(
    uint32_t lba,
    const uint8_t *buffer
);

#endif
