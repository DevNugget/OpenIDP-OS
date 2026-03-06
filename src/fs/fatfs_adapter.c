#include <fs/fatfs_adapter.h>

#include <drivers/com1.h>
#include <fs/vfs.h>
#include <fs/fatfs/ff.h>
#include <memory/kheap.h>
#include <utility/kstring.h>
#include <syscall/syscall.h>

static FATFS g_fatfs;

typedef struct {
    char mount_point[16];
} fatfs_context_t;

static fatfs_context_t g_fatfs_ctx;

typedef enum {
    FATFS_HANDLE_FILE = 1,
    FATFS_HANDLE_DIR = 2
} fatfs_handle_type_t;

typedef struct {
    fatfs_handle_type_t type;
    union {
        FIL fil;
        DIR dir;
    } u;
} fatfs_handle_t;

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

    fatfs_handle_t* handle = (fatfs_handle_t*)kmalloc(sizeof(fatfs_handle_t));
    if (!handle) {
        return VFS_ERR_NO_SPACE;
    }

    char fat_path[96];
    if (strlen(rel_path) + 3 >= sizeof(fat_path)) {
        kfree(handle);
        return VFS_ERR_INVALID;
    }

    fat_path[0] = '0';
    fat_path[1] = ':';
    strcpy(&fat_path[2], rel_path);

    if ((flags & IDP_O_DIRECTORY) != 0u) {
        FRESULT dresult = f_opendir(&handle->u.dir, fat_path);
        if (dresult != FR_OK) {
            kfree(handle);
            return VFS_ERR_NOT_FOUND;
        }
        handle->type = FATFS_HANDLE_DIR;
        *out_handle = handle;
        return VFS_OK;
    }

    uint8_t mode = 0;
    if ((flags & IDP_O_RDONLY) != 0u) {
        mode |= FA_READ;
    }
    if ((flags & IDP_O_WRONLY) != 0u) {
        mode |= FA_WRITE;
    }
    if ((flags & IDP_O_CREATE) != 0u) {
        mode |= FA_CREATE_ALWAYS;
    }

    FRESULT result = f_open(&handle->u.fil, fat_path, mode);
    if (result != FR_OK) {
        kfree(handle);
        return VFS_ERR_NOT_FOUND;
    }

    handle->type = FATFS_HANDLE_FILE;
    *out_handle = handle;
    return VFS_OK;
}

static vfs_status_t fatfs_readdir(void* fs_context, void* dir_handle, void* out_dirent) {
    (void)fs_context;

    fatfs_handle_t* handle = (fatfs_handle_t*)dir_handle;
    idp_dirent_t* entry = (idp_dirent_t*)out_dirent;
    if (!handle || !entry || handle->type != FATFS_HANDLE_DIR) {
        return VFS_ERR_INVALID;
    }

    FILINFO info;
    FRESULT result = f_readdir(&handle->u.dir, &info);
    if (result != FR_OK) {
        return VFS_ERR_IO;
    }

    if (info.fname[0] == '\0') {
        entry->name[0] = '\0';
        entry->type = 0;
        return VFS_OK;
    }

    strncpy(entry->name, info.fname, IDP_DIRENT_NAME_MAX - 1);
    entry->name[IDP_DIRENT_NAME_MAX - 1] = '\0';
    entry->type = (info.fattrib & AM_DIR) ? IDP_DIRENT_TYPE_DIR : IDP_DIRENT_TYPE_FILE;
    return VFS_OK;
}

static vfs_status_t fatfs_read(void* fs_context, void* file_handle, void* buffer, size_t bytes, size_t* out_read) {
    (void)fs_context;

    fatfs_handle_t* handle = (fatfs_handle_t*)file_handle;
    if (!handle || !buffer || handle->type != FATFS_HANDLE_FILE) {
        return VFS_ERR_INVALID;
    }

    UINT br = 0;
    FRESULT result = f_read(&handle->u.fil, buffer, (UINT)bytes, &br);
    if (out_read) {
        *out_read = br;
    }

    return result == FR_OK ? VFS_OK : VFS_ERR_IO;
}

static vfs_status_t fatfs_write(void* fs_context, void* file_handle, const void* buffer, size_t bytes, size_t* out_written) {
    (void)fs_context;

    fatfs_handle_t* handle = (fatfs_handle_t*)file_handle;
    if (!handle || !buffer || handle->type != FATFS_HANDLE_FILE) {
        return VFS_ERR_INVALID;
    }

    UINT bw = 0;
    FRESULT result = f_write(&handle->u.fil, buffer, (UINT)bytes, &bw);
    if (out_written) {
        *out_written = bw;
    }

    return result == FR_OK ? VFS_OK : VFS_ERR_IO;
}

static vfs_status_t fatfs_close(void* fs_context, void* file_handle) {
    (void)fs_context;

    fatfs_handle_t* handle = (fatfs_handle_t*)file_handle;
    if (!handle) {
        return VFS_ERR_INVALID;
    }

    FRESULT result = FR_OK;
    if (handle->type == FATFS_HANDLE_FILE) {
        result = f_close(&handle->u.fil);
    } else if (handle->type == FATFS_HANDLE_DIR) {
        result = f_closedir(&handle->u.dir);
    }

    kfree(handle);
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
        .readdir = fatfs_readdir,
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