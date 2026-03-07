#include <fs/vfs.h>
#include <drivers/com1.h>
#include <utility/kstring.h>
#include <memory/kheap.h>

typedef struct {
    bool in_use;
    char mount_point[16];
    const vfs_filesystem_ops_t* ops;
} vfs_mount_t;

typedef enum { VFS_TYPE_FILE = 0, VFS_TYPE_PIPE } vfs_file_type_t;

typedef struct {
    uint8_t buffer[4096];
    volatile size_t head;
    volatile size_t tail;
    volatile int ref_count;
} pipe_t;

struct vfs_file {
    bool in_use;
    vfs_file_type_t type;
    const vfs_mount_t* mount;
    void* handle;
    bool is_pipe_write_end;
    int ref_count;
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
    if (!mount_point || mount_point[0] != '/' || !fs_ops || !fs_ops->open || !fs_ops->readdir || !fs_ops->read || !fs_ops->write || !fs_ops->close) {
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
        g_files[i].type = VFS_TYPE_FILE;
        g_files[i].ref_count = 1;
        *out_file = &g_files[i];
        return VFS_OK;
    }

    return VFS_ERR_NO_SPACE;
}

vfs_status_t vfs_create_pipe(vfs_file_t** out_read, vfs_file_t** out_write) {
    int r_idx = -1, w_idx = -1;
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (!g_files[i].in_use) {
            if (r_idx == -1) r_idx = i;
            else if (w_idx == -1) { w_idx = i; break; }
        }
    }
    if (r_idx == -1 || w_idx == -1) return VFS_ERR_NO_SPACE;

    pipe_t* p = kmalloc(sizeof(pipe_t));
    if (!p) return VFS_ERR_NO_SPACE;
    memset(p, 0, sizeof(pipe_t));
    p->ref_count = 2;

    g_files[r_idx].in_use = true;
    g_files[r_idx].ref_count = 1;
    g_files[r_idx].type = VFS_TYPE_PIPE;
    g_files[r_idx].handle = p;
    g_files[r_idx].is_pipe_write_end = false;
    *out_read = &g_files[r_idx];

    g_files[w_idx].in_use = true;
    g_files[w_idx].ref_count = 1;
    g_files[w_idx].type = VFS_TYPE_PIPE;
    g_files[w_idx].handle = p;
    g_files[w_idx].is_pipe_write_end = true;
    *out_write = &g_files[w_idx];

    return VFS_OK;
}

vfs_status_t vfs_readdir(vfs_file_t* file, void* out_dirent) {
    if (!file || !file->in_use || !out_dirent) {
        return VFS_ERR_INVALID;
    }

    if (file->type == VFS_TYPE_PIPE) {
        return VFS_ERR_INVALID;
    }

    return file->mount->ops->readdir(file->mount->ops->fs_context, file->handle, out_dirent);
}

vfs_status_t vfs_read(vfs_file_t* file, void* buffer, size_t bytes, size_t* out_read) {
    if (!file || !file->in_use) return VFS_ERR_INVALID;
    
    if (file->type == VFS_TYPE_PIPE) {
        if (file->is_pipe_write_end) return VFS_ERR_INVALID;
        pipe_t* p = (pipe_t*)file->handle;
        size_t rd = 0;
        uint8_t* b = (uint8_t*)buffer;
        while (rd < bytes && p->head != p->tail) {
            b[rd++] = p->buffer[p->tail];
            p->tail = (p->tail + 1) % sizeof(p->buffer);
        }
        *out_read = rd;
        return VFS_OK;
    }
    return file->mount->ops->read(file->mount->ops->fs_context, file->handle, buffer, bytes, out_read);
}

vfs_status_t vfs_write(vfs_file_t* file, const void* buffer, size_t bytes, size_t* out_written) {
    if (!file || !file->in_use) return VFS_ERR_INVALID;
    
    if (file->type == VFS_TYPE_PIPE) {
        if (!file->is_pipe_write_end) return VFS_ERR_INVALID;
        pipe_t* p = (pipe_t*)file->handle;
        size_t wr = 0;
        const uint8_t* b = (const uint8_t*)buffer;
        while (wr < bytes) {
            size_t next_head = (p->head + 1) % sizeof(p->buffer);
            if (next_head == p->tail) break;
            p->buffer[p->head] = b[wr++];
            p->head = next_head;
        }
        *out_written = wr;
        return VFS_OK;
    }
    return file->mount->ops->write(file->mount->ops->fs_context, file->handle, buffer, bytes, out_written);
}

vfs_status_t vfs_close(vfs_file_t* file) {
    if (!file || !file->in_use) return VFS_ERR_INVALID;
    
    file->ref_count--;
    if (file->ref_count > 0) {
        return VFS_OK;
    }

    if (file->type == VFS_TYPE_PIPE) {
        pipe_t* p = (pipe_t*)file->handle;
        p->ref_count--;
        if (p->ref_count == 0) kfree(p);
    } else {
        file->mount->ops->close(file->mount->ops->fs_context, file->handle);
    }
    
    file->in_use = false;
    file->mount = NULL;
    file->handle = NULL;
    return VFS_OK;
}

void vfs_file_inc_ref(vfs_file_t* file) {
    if (file && file->in_use) {
        file->ref_count++;
    }
}
