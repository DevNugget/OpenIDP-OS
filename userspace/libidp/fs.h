#ifndef LIBIDP_FS_H
#define LIBIDP_FS_H

#include <stdint.h>
#include <stddef.h>
#include <libidp/syscall.h>

typedef struct idp_file_t {
    uint64_t fd;
    int is_open;
} idp_file_t;

typedef struct idp_dir_t {
    uint64_t fd;
    int is_open;
} idp_dir_t;

typedef struct idp_dir_entry_t {
    uint8_t type;
    char name[IDP_DIRENT_NAME_MAX];
} idp_dir_entry_t;

int idp_fs_open(idp_file_t* out_file, const char* path, uint32_t flags);
int idp_fs_read(idp_file_t* file, void* buffer, size_t bytes, size_t* out_read);
int idp_fs_close(idp_file_t* file);

int idp_fs_opendir(idp_dir_t* out_dir, const char* path);
int idp_fs_readdir(idp_dir_t* dir, idp_dir_entry_t* out_entry, int* out_has_entry);
int idp_fs_closedir(idp_dir_t* dir);

#endif
