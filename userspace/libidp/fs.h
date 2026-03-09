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

typedef idp_file_t file_t;
typedef idp_dir_t dir_t;
typedef idp_dir_entry_t dir_entry_t;

int fs_open(file_t* out_file, const char* path, uint32_t flags);
int fs_read(file_t* file, void* buffer, size_t bytes, size_t* out_read);
int fs_write(file_t* file, const void* buffer, size_t bytes, size_t* out_written);
int fs_close(file_t* file);

int fs_opendir(dir_t* out_dir, const char* path);
int fs_readdir(dir_t* dir, dir_entry_t* out_entry, int* out_has_entry);
int fs_closedir(dir_t* dir);

int fs_pipe(file_t* out_read_end, file_t* out_write_end);
int fs_dup2(const file_t* from, file_t* to, uint64_t target_fd);

int fs_getcwd(char* buffer, size_t size);
int fs_chdir(const char* path);

int idp_fs_open(idp_file_t* out_file, const char* path, uint32_t flags);
int idp_fs_read(idp_file_t* file, void* buffer, size_t bytes, size_t* out_read);
int idp_fs_write(idp_file_t* file, const void* buffer, size_t bytes, size_t* out_written);
int idp_fs_close(idp_file_t* file);

int idp_fs_opendir(idp_dir_t* out_dir, const char* path);
int idp_fs_readdir(idp_dir_t* dir, idp_dir_entry_t* out_entry, int* out_has_entry);
int idp_fs_closedir(idp_dir_t* dir);
int idp_fs_pipe(idp_file_t* out_read_end, idp_file_t* out_write_end);
int idp_fs_dup2(const idp_file_t* from, idp_file_t* to, uint64_t target_fd);
int idp_fs_getcwd(char* buffer, size_t size);
int idp_fs_chdir(const char* path);

#endif
