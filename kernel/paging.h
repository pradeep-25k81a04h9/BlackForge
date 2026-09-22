#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_SIZE 4096

#define PAGE_DIRECTORY_ADDRESS 0x00040000
#define PAGE_TABLES_ADDRESS    0x00041000

#define PAGING_MEMORY_SIZE     0x01000000
#define PAGING_TABLE_COUNT     4

void paging_init(void);

int paging_test(void);

int map_page(uint32_t virtual_address,
             uint32_t physical_address);

int unmap_page(uint32_t virtual_address);

#endif
