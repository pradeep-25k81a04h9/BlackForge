#include "storage.h"
static int storage_available = 0;
#define ATA_PRIMARY_IO       0x1F0
#define ATA_PRIMARY_CONTROL  0x3F6
#define ATA_CMD_CACHE_FLUSH  0xE7

#define ATA_REG_DATA         0
#define ATA_REG_ERROR        1
#define ATA_REG_SECCOUNT0    2
#define ATA_REG_LBA0         3
#define ATA_REG_LBA1         4
#define ATA_REG_LBA2         5
#define ATA_REG_HDDEVSEL     6
#define ATA_REG_COMMAND      7
#define ATA_REG_STATUS       7

#define ATA_CMD_READ_PIO     0x20
#define ATA_CMD_WRITE_PIO    0x30
#define ATA_CMD_IDENTIFY     0xEC

#define ATA_SR_BSY           0x80
#define ATA_SR_DRDY          0x40
#define ATA_SR_DRQ           0x08
#define ATA_SR_ERR           0x01

static void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static void insw(
    uint16_t port,
    uint16_t *buffer,
    unsigned int count
)
{
    __asm__ volatile (
        "cld; rep insw"
        : "+D"(buffer), "+c"(count)
        : "d"(port)
        : "memory"
    );
}

static void outsw(
    uint16_t port,
    const uint16_t *buffer,
    unsigned int count
)
{
    __asm__ volatile (
        "cld; rep outsw"
        : "+S"(buffer), "+c"(count)
        : "d"(port)
        : "memory"
    );
}

static int ata_wait_ready(void)
{
    unsigned int timeout = 1000000;

    while (timeout--)
    {
        uint8_t status =
            inb(ATA_PRIMARY_IO + ATA_REG_STATUS);

        if (status & ATA_SR_ERR)
            return -1;

        if (!(status & ATA_SR_BSY) &&
            (status & ATA_SR_DRDY))
        {
            return 0;
        }
    }

    return -1;
}

static int ata_wait_drq(void)
{
    unsigned int timeout = 1000000;

    while (timeout--)
    {
        uint8_t status =
            inb(ATA_PRIMARY_IO + ATA_REG_STATUS);

        if (status & ATA_SR_ERR)
            return -1;

        if (!(status & ATA_SR_BSY) &&
            (status & ATA_SR_DRQ))
        {
            return 0;
        }
    }

    return -1;
}

int storage_init(void)
{
    storage_available = 0;

    outb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, 0xA0);

    inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    inb(ATA_PRIMARY_IO + ATA_REG_STATUS);

    if (ata_wait_ready() != 0)
        return -1;

    storage_available = 1;
    return 0;
}
int storage_is_available(void)
{
    return storage_available;
}
int storage_read_sector(
    uint32_t lba,
    uint8_t *buffer
)
{
    uint8_t device;

    if (!buffer)
        return -1;

    if (ata_wait_ready() != 0)
        return -1;

    device =
        0xE0 |
        ((lba >> 24) & 0x0F);

    outb(
        ATA_PRIMARY_IO + ATA_REG_HDDEVSEL,
        device
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_SECCOUNT0,
        1
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_LBA0,
        (uint8_t)(lba & 0xFF)
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_LBA1,
        (uint8_t)((lba >> 8) & 0xFF)
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_LBA2,
        (uint8_t)((lba >> 16) & 0xFF)
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_COMMAND,
        ATA_CMD_READ_PIO
    );

    if (ata_wait_drq() != 0)
        return -1;

    insw(
        ATA_PRIMARY_IO + ATA_REG_DATA,
        (uint16_t *)buffer,
        STORAGE_SECTOR_SIZE / 2
    );

    return 0;
}

int storage_write_sector(
    uint32_t lba,
    const uint8_t *buffer
)
{
    uint8_t device;

    if (!buffer)
        return -1;

    if (ata_wait_ready() != 0)
        return -1;

    device =
        0xE0 |
        ((lba >> 24) & 0x0F);

    outb(
        ATA_PRIMARY_IO + ATA_REG_HDDEVSEL,
        device
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_SECCOUNT0,
        1
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_LBA0,
        (uint8_t)(lba & 0xFF)
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_LBA1,
        (uint8_t)((lba >> 8) & 0xFF)
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_LBA2,
        (uint8_t)((lba >> 16) & 0xFF)
    );

    outb(
        ATA_PRIMARY_IO + ATA_REG_COMMAND,
        ATA_CMD_WRITE_PIO
    );

    if (ata_wait_drq() != 0)
        return -1;

    outsw(
        ATA_PRIMARY_IO + ATA_REG_DATA,
        (const uint16_t *)buffer,
        STORAGE_SECTOR_SIZE / 2
    );

    /*
     * Wait until the controller finishes.
     */
    /*
     * Flush the written sector to the disk.
     */
    outb(
        ATA_PRIMARY_IO + ATA_REG_COMMAND,
        ATA_CMD_CACHE_FLUSH
    );

    /*
     * Wait until the controller finishes.
     */
    if (ata_wait_ready() != 0)
        return -1;

    return 0;
}
