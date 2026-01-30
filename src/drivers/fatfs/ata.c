#include <stdint.h>
#include <io.h>
#include <com1.h>
#include <fatfs/ata.h>

static void ata_wait() {
    uint8_t status;

    // Wait for BSY to clear
    do {
        status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    } while (status & ATA_SR_BSY);

    //terminal_writestring("BSY cleared.\n");

    // Now wait for DRQ
    int timeout = 100000;
    while (!(status & ATA_SR_DRQ) && timeout--) {
        status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) {
            serial_printf("ATA error during wait!\n");
            break;
        }
    }
}

void ata_read_sector(uint32_t lba, uint8_t* buf) {
    outb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, ATA_MASTER | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT0, 1);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA0, lba & 0xFF);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA1, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA2, (lba >> 16) & 0xFF);
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    ata_wait();

    for (int i = 0; i < 256; i++) {
        ((uint16_t*)buf)[i] = inw(ATA_PRIMARY_IO + ATA_REG_DATA);
    }
}

void ata_write_sector(uint32_t lba, const uint8_t* buf) {
    // 1. Wait until drive is not busy
    while (inb(ATA_PRIMARY_IO + ATA_REG_STATUS) & ATA_SR_BSY);

    // 2. Select drive and LBA mode
    outb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, ATA_MASTER | ((lba >> 24) & 0x0F));

    // 3. Send sector count and LBA
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT0, 1);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA0, lba & 0xFF);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA1, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA2, (lba >> 16) & 0xFF);

    // 4. Send write command
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    // 5. Wait for DRQ or ERR
    uint8_t status;
    do {
        status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) {
            serial_printf("ATA error: can't write sector\n");
            return;
        }
    } while (!(status & ATA_SR_DRQ));

    // 6. Write 256 words (512 bytes)
    for (int i = 0; i < 256; i++) {
        outw(ATA_PRIMARY_IO + ATA_REG_DATA, ((uint16_t*)buf)[i]);
    }

    // 7. Flush (optional for now)
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    while (inb(ATA_PRIMARY_IO + ATA_REG_STATUS) & ATA_SR_BSY);
}