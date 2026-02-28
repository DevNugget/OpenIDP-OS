#include <fs/fatfs_adapter.h>

#include <drivers/com1.h>
#include <fs/vfs.h>
#include <fs/fatfs/ff.h>
#include <memory/kheap.h>
#include <utility/kstring.h>

static FATFS g_fatfs;

typedef struct {
    char mount_point[16];
} fatfs_context_t;

static fatfs_context_t g_fatfs_ctx;

typedef struct {
    FIL fil;
} fatfs_file_t;

static const char* fatfs_to_relative_path(const fatfs_context_t* ctx, const char* path) {
    if (!ctx || !path) {
        return NULL;
    }

    size_t mount_len = strlen(ctx->mount_point);
    if (strncmp(path, ctx->mount_point, mount_len) != 0) {
        return NULL;
    }

    const char* rel = path + mount_len;
    if (*rel == '\0') {
        return "/";
    }

    if (*rel != '/') {
        return NULL;
    }

    return rel;
}

static vfs_status_t fatfs_open(void* fs_context, const char* path, uint32_t flags, void** out_handle) {
    fatfs_context_t* ctx = (fatfs_context_t*)fs_context;

    if (!path || !out_handle || !ctx) {
        return VFS_ERR_INVALID;
    }

    const char* rel_path = fatfs_to_relative_path(ctx, path);
    if (!rel_path) {
        return VFS_ERR_INVALID;
    }

    fatfs_file_t* file = (fatfs_file_t*)kmalloc(sizeof(fatfs_file_t));
    if (!file) {
        return VFS_ERR_NO_SPACE;
    }

    uint8_t mode = 0;
    if (flags & 0x1) {
        mode |= FA_READ;
    }
    if (flags & 0x2) {
        mode |= FA_WRITE;
    }
    if (flags & 0x4) {
        mode |= FA_CREATE_ALWAYS;
    }

    char fat_path[96];
    if (strlen(rel_path) + 3 >= sizeof(fat_path)) {
        kfree(file);
        return VFS_ERR_INVALID;
    }

    fat_path[0] = '0';
    fat_path[1] = ':';
    strcpy(&fat_path[2], rel_path);

    FRESULT result = f_open(&file->fil, fat_path, mode);
    if (result != FR_OK) {
        kfree(file);
        return VFS_ERR_NOT_FOUND;
    }

    *out_handle = file;
    return VFS_OK;
}

static vfs_status_t fatfs_read(void* fs_context, void* file_handle, void* buffer, size_t bytes, size_t* out_read) {
    (void)fs_context;

    fatfs_file_t* file = (fatfs_file_t*)file_handle;
    if (!file || !buffer) {
        return VFS_ERR_INVALID;
    }

    UINT br = 0;
    FRESULT result = f_read(&file->fil, buffer, (UINT)bytes, &br);
    if (out_read) {
        *out_read = br;
    }

    return result == FR_OK ? VFS_OK : VFS_ERR_IO;
}

static vfs_status_t fatfs_write(void* fs_context, void* file_handle, const void* buffer, size_t bytes, size_t* out_written) {
    (void)fs_context;

    fatfs_file_t* file = (fatfs_file_t*)file_handle;
    if (!file || !buffer) {
        return VFS_ERR_INVALID;
    }

    UINT bw = 0;
    FRESULT result = f_write(&file->fil, buffer, (UINT)bytes, &bw);
    if (out_written) {
        *out_written = bw;
    }

    return result == FR_OK ? VFS_OK : VFS_ERR_IO;
}

static vfs_status_t fatfs_close(void* fs_context, void* file_handle) {
    (void)fs_context;

    fatfs_file_t* file = (fatfs_file_t*)file_handle;
    if (!file) {
        return VFS_ERR_INVALID;
    }

    FRESULT result = f_close(&file->fil);
    kfree(file);
    return result == FR_OK ? VFS_OK : VFS_ERR_IO;
}

bool fatfs_mount_nvme(const char* mount_point) {
    if (!mount_point || mount_point[0] != '/') {
        serial_write_str("[FATFS] invalid mount point\n");
        return false;
    }

    size_t mount_len = strlen(mount_point);
    if (mount_len == 0 || mount_len >= sizeof(g_fatfs_ctx.mount_point)) {
        serial_write_str("[FATFS] mount point too long\n");
        return false;
    }

    FRESULT mount_result = f_mount(&g_fatfs, "0:", 1);
    if (mount_result != FR_OK) {
        serial_printf("[FATFS] mount failed: %d\n", mount_result);
        return false;
    }

    strncpy(g_fatfs_ctx.mount_point, mount_point, sizeof(g_fatfs_ctx.mount_point) - 1);
    g_fatfs_ctx.mount_point[sizeof(g_fatfs_ctx.mount_point) - 1] = '\0';

    static const vfs_filesystem_ops_t fatfs_ops = {
        .name = "fatfs",
        .fs_context = &g_fatfs_ctx,
        .open = fatfs_open,
        .read = fatfs_read,
        .write = fatfs_write,
        .close = fatfs_close
    };

    vfs_status_t mount_status = vfs_mount(mount_point, &fatfs_ops);
    if (mount_status != VFS_OK) {
        serial_printf("[FATFS] VFS mount failed: %d\n", mount_status);
        return false;
    }

    serial_printf("[FATFS] mounted NVMe volume at %s\n", mount_point);
    return true;
}
