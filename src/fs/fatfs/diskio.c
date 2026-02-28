#include <fs/fatfs/diskio.h>
#include <drivers/nvme.h>

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) {
        return STA_NOINIT;
    }

    if (!nvme_is_ready()) {
        return STA_NOINIT;
    }

    return 0;
}

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0 || !nvme_is_ready()) {
        return STA_NOINIT;
    }

    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || !buff || count == 0) {
        return RES_PARERR;
    }

    if (!nvme_read_blocks(sector, count, buff)) {
        return RES_ERROR;
    }

    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || !buff || count == 0) {
        return RES_PARERR;
    }

    if (!nvme_write_blocks(sector, count, buff)) {
        return RES_ERROR;
    }

    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv != 0) {
        return RES_PARERR;
    }

    const nvme_namespace_t* ns = nvme_get_namespace();
    if (!ns) {
        return RES_ERROR;
    }

    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;
        case GET_SECTOR_COUNT: {
            if (!buff) {
                return RES_PARERR;
            }
            uint64_t total = ns->total_blocks;
            *(uint32_t*)buff = total > 0xFFFFFFFFULL ? 0xFFFFFFFFU : (uint32_t)total;
            return RES_OK;
        }
        case GET_SECTOR_SIZE:
            if (!buff) {
                return RES_PARERR;
            }
            *(uint16_t*)buff = (uint16_t)ns->block_size;
            return RES_OK;
        case GET_BLOCK_SIZE:
            if (!buff) {
                return RES_PARERR;
            }
            *(uint32_t*)buff = 1;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}
