#include <fs/vfs.h>
#include <drivers/com1.h>
#include <utility/kstring.h>

typedef struct {
    bool in_use;
    char mount_point[16];
    const vfs_filesystem_ops_t* ops;
} vfs_mount_t;

struct vfs_file {
    bool in_use;
    const vfs_mount_t* mount;
    void* handle;
};

static vfs_mount_t g_mounts[VFS_MAX_MOUNTS];
static struct vfs_file g_files[VFS_MAX_OPEN_FILES];

static const vfs_mount_t* vfs_find_mount(const char* path) {
    const vfs_mount_t* best = NULL;
    size_t best_len = 0;

    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!g_mounts[i].in_use) {
            continue;
        }

        size_t mount_len = strlen(g_mounts[i].mount_point);
        if (mount_len <= best_len) {
            continue;
        }

        if (strncmp(path, g_mounts[i].mount_point, mount_len) != 0) {
            continue;
        }

        char boundary = path[mount_len];
        if (boundary != '\0' && boundary != '/') {
            continue;
        }

        best = &g_mounts[i];
        best_len = mount_len;
    }

    return best;
}

void vfs_init(void) {
    memset(g_mounts, 0, sizeof(g_mounts));
    memset(g_files, 0, sizeof(g_files));
}

vfs_status_t vfs_mount(const char* mount_point, const vfs_filesystem_ops_t* fs_ops) {
    if (!mount_point || mount_point[0] != '/' || !fs_ops || !fs_ops->open || !fs_ops->read || !fs_ops->write || !fs_ops->close) {
        return VFS_ERR_INVALID;
    }

    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!g_mounts[i].in_use) {
            g_mounts[i].in_use = true;
            strncpy(g_mounts[i].mount_point, mount_point, sizeof(g_mounts[i].mount_point) - 1);
            g_mounts[i].mount_point[sizeof(g_mounts[i].mount_point) - 1] = '\0';
            g_mounts[i].ops = fs_ops;
            serial_printf("[VFS] Mounted '%s' at %s\n", fs_ops->name ? fs_ops->name : "unknown", g_mounts[i].mount_point);
            return VFS_OK;
        }
    }

    return VFS_ERR_NO_SPACE;
}

vfs_status_t vfs_open(const char* path, uint32_t flags, vfs_file_t** out_file) {
    if (!path || !out_file) {
        return VFS_ERR_INVALID;
    }

    const vfs_mount_t* mount = vfs_find_mount(path);
    if (!mount) {
        return VFS_ERR_NOT_FOUND;
    }

    for (size_t i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (g_files[i].in_use) {
            continue;
        }

        void* handle = NULL;
        vfs_status_t status = mount->ops->open(mount->ops->fs_context, path, flags, &handle);
        if (status != VFS_OK) {
            return status;
        }

        g_files[i].in_use = true;
        g_files[i].mount = mount;
        g_files[i].handle = handle;
        *out_file = &g_files[i];
        return VFS_OK;
    }

    return VFS_ERR_NO_SPACE;
}

vfs_status_t vfs_read(vfs_file_t* file, void* buffer, size_t bytes, size_t* out_read) {
    if (!file || !file->in_use) {
        return VFS_ERR_INVALID;
    }

    return file->mount->ops->read(file->mount->ops->fs_context, file->handle, buffer, bytes, out_read);
}

vfs_status_t vfs_write(vfs_file_t* file, const void* buffer, size_t bytes, size_t* out_written) {
    if (!file || !file->in_use) {
        return VFS_ERR_INVALID;
    }

    return file->mount->ops->write(file->mount->ops->fs_context, file->handle, buffer, bytes, out_written);
}

vfs_status_t vfs_close(vfs_file_t* file) {
    if (!file || !file->in_use) {
        return VFS_ERR_INVALID;
    }

    vfs_status_t status = file->mount->ops->close(file->mount->ops->fs_context, file->handle);
    file->in_use = false;
    file->mount = NULL;
    file->handle = NULL;
    return status;
}
