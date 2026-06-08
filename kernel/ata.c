#include "ata.h"
#include "../drivers/pic.h"

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define DRIVE_SELECT (ATA_USE_SLAVE ? 0xF0 : 0xE0)

static void ata_delay() {
    for (int i = 0; i < 4; i++) inb(ATA_STATUS);
}

static int ata_poll() {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return 0;
    }
    return -1;
}

static void ata_wait_bsy() {
    for (int i = 0; i < 100000; i++) {
        if (!(inb(ATA_STATUS) & ATA_SR_BSY)) return;
    }
}

int ata_init() {
    ata_wait_bsy();
    outb(ATA_DRIVE, (uint8_t)DRIVE_SELECT);
    ata_delay();

    outb(ATA_SECCOUNT, 0);
    outb(ATA_LBA_LO, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t status = inb(ATA_STATUS);
    if (status == 0) return 0;

    ata_wait_bsy();


    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HI) != 0) return 0;

    for (int i = 0; i < 100000; i++) {
        status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR) return 0;
        if (status & ATA_SR_DRQ) break;
    }


    for (int i = 0; i < 256; i++) inw(ATA_DATA);
    return 1;
}

int ata_read_sector(uint32_t lba, uint8_t* buf) {
    ata_wait_bsy();
    outb(ATA_DRIVE, (uint8_t)(DRIVE_SELECT | ((lba >> 24) & 0x0F)));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, ATA_CMD_READ);

    if (ata_poll() != 0) return -1;

    uint16_t* wbuf = (uint16_t*) buf;
    for (int i = 0; i < 256; i++) wbuf[i] = inw(ATA_DATA);
    return 0;
}

int ata_write_sector(uint32_t lba, const uint8_t* buf) {
    ata_wait_bsy();
    outb(ATA_DRIVE, (uint8_t)(DRIVE_SELECT | ((lba >> 24) & 0x0F)));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND, ATA_CMD_WRITE);

    if (ata_poll() != 0) return -1;

    const uint16_t* wbuf = (const uint16_t*) buf;
    for (int i = 0; i < 256; i++) outw(ATA_DATA, wbuf[i]);


    outb(ATA_COMMAND, ATA_CMD_FLUSH);
    ata_wait_bsy();
    return 0;
}
