#ifndef ATA_H
#define ATA_H

#include "../include/types.h"

#define ATA_SECTOR_SIZE 512

// Primary ATA bus I/O ports
#define ATA_DATA        0x1F0
#define ATA_FEATURES    0x1F1
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LO      0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HI      0x1F5
#define ATA_DRIVE       0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7
#define ATA_CONTROL     0x3F6

// Status register bits
#define ATA_SR_BSY      0x80
#define ATA_SR_DRDY     0x40
#define ATA_SR_DRQ      0x08
#define ATA_SR_ERR      0x01

// Commands
#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_CMD_FLUSH   0xE7
#define ATA_CMD_IDENTIFY 0xEC

// We use the primary-slave disk as the writable scratch drive,
// keeping the boot disk (primary master) safe.
#define ATA_USE_SLAVE   1

int  ata_init();                                  // returns 1 if disk present
int  ata_read_sector(uint32_t lba, uint8_t* buf);
int  ata_write_sector(uint32_t lba, const uint8_t* buf);

#endif
