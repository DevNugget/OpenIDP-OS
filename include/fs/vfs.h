#ifndef VFS_H
#define VFS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define VFS_MAX_MOUNTS 8
#define VFS_MAX_OPEN_FILES 64

typedef enum {
    VFS_OK = 0,
    VFS_ERR_GENERIC = -1,
    VFS_ERR_INVALID = -2,
    VFS_ERR_NO_SPACE = -3,
    VFS_ERR_NOT_FOUND = -4,
    VFS_ERR_IO = -5
} vfs_status_t;

typedef struct vfs_file vfs_file_t;

typedef struct {
    const char* name;
    void* fs_context;
    vfs_status_t (*open)(void* fs_context, const char* path, uint32_t flags, void** out_handle);
    vfs_status_t (*readdir)(void* fs_context, void* dir_handle, void* out_dirent);
    vfs_status_t (*read)(void* fs_context, void* file_handle, void* buffer, size_t bytes, size_t* out_read);
    vfs_status_t (*write)(void* fs_context, void* file_handle, const void* buffer, size_t bytes, size_t* out_written);
    vfs_status_t (*close)(void* fs_context, void* file_handle);
} vfs_filesystem_ops_t;

void vfs_init(void);
vfs_status_t vfs_mount(const char* mount_point, const vfs_filesystem_ops_t* fs_ops);
vfs_status_t vfs_open(const char* path, uint32_t flags, vfs_file_t** out_file);
vfs_status_t vfs_create_pipe(vfs_file_t** out_read, vfs_file_t** out_write);
vfs_status_t vfs_readdir(vfs_file_t* file, void* out_dirent);
vfs_status_t vfs_read(vfs_file_t* file, void* buffer, size_t bytes, size_t* out_read);
vfs_status_t vfs_write(vfs_file_t* file, const void* buffer, size_t bytes, size_t* out_written);
vfs_status_t vfs_close(vfs_file_t* file);
void vfs_file_inc_ref(vfs_file_t* file);

#endif //VFS_H
