#include <fatfs/ff.h>         /* Obtains integer types */
#include <fatfs/diskio.h>
#include <fatfs/ata.h>
#include <com1.h>
#include <stdint.h>
#include <kstring.h>

DSTATUS disk_initialize(BYTE pdrv) {
    return 0; // Assume always ready for now
}

DSTATUS disk_status(BYTE pdrv) {
    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    for (UINT i = 0; i < count; i++) {
        ata_read_sector((uint32_t)(sector + i), buff + (i * 512));
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    serial_printf("Sector: 0x%x\nCount: 0x%x\n");

    for (UINT i = 0; i < count; i++) {
        ata_write_sector((uint32_t)(sector + i), buff + (i * 512));
    }


    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    return RES_OK; // Stubbed
}