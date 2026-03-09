#include <libidp/fs.h>

static int file_from_fd(file_t* out_file, uint64_t fd) {
    if (out_file == NULL || fd == (uint64_t)ERR_FAIL) {
        return ERR_FAIL;
    }

    out_file->fd = fd;
    out_file->is_open = 1;
    return ERR_SUCCESS;
}

int fs_open(file_t* out_file, const char* path, uint32_t flags) {
    if (out_file == NULL || path == NULL) {
        return ERR_FAIL;
    }

    return file_from_fd(out_file, sys_open(path, flags));
}

int fs_read(file_t* file, void* buffer, size_t bytes, size_t* out_read) {
    if (file == NULL || !file->is_open || buffer == NULL || out_read == NULL) {
        return ERR_FAIL;
    }

    uint64_t read_bytes = 0;
    if (sys_read(file->fd, buffer, (uint64_t)bytes, &read_bytes) != ERR_SUCCESS) {
        return ERR_FAIL;
    }

    *out_read = (size_t)read_bytes;
    return ERR_SUCCESS;
}

int fs_write(file_t* file, const void* buffer, size_t bytes, size_t* out_written) {
    if (file == NULL || !file->is_open || buffer == NULL || out_written == NULL) {
        return ERR_FAIL;
    }

    uint64_t written_bytes = 0;
    if (sys_write(file->fd, buffer, (uint64_t)bytes, &written_bytes) != ERR_SUCCESS) {
        return ERR_FAIL;
    }

    *out_written = (size_t)written_bytes;
    return ERR_SUCCESS;
}

int fs_close(file_t* file) {
    if (file == NULL || !file->is_open) {
        return ERR_FAIL;
    }

    if (sys_close(file->fd) != ERR_SUCCESS) {
        return ERR_FAIL;
    }

    file->is_open = 0;
    file->fd = 0;
    return ERR_SUCCESS;
}

int fs_opendir(dir_t* out_dir, const char* path) {
    if (out_dir == NULL || path == NULL) {
        return ERR_FAIL;
    }

    uint64_t fd = sys_open(path, IDP_O_DIRECTORY | IDP_O_RDONLY);
    if (fd == (uint64_t)ERR_FAIL) {
        return ERR_FAIL;
    }

    out_dir->fd = fd;
    out_dir->is_open = 1;
    return ERR_SUCCESS;
}

int fs_readdir(dir_t* dir, dir_entry_t* out_entry, int* out_has_entry) {
    if (dir == NULL || !dir->is_open || out_entry == NULL || out_has_entry == NULL) {
        return ERR_FAIL;
    }

    idp_dirent_t raw_entry;
    if (sys_readdir(dir->fd, &raw_entry) != ERR_SUCCESS) {
        return ERR_FAIL;
    }

    if (raw_entry.name[0] == '\0') {
        *out_has_entry = 0;
        return ERR_SUCCESS;
    }

    out_entry->type = raw_entry.type;
    for (size_t i = 0; i < IDP_DIRENT_NAME_MAX; ++i) {
        out_entry->name[i] = raw_entry.name[i];
        if (raw_entry.name[i] == '\0') {
            break;
        }
    }

    *out_has_entry = 1;
    return ERR_SUCCESS;
}

int fs_closedir(dir_t* dir) {
    if (dir == NULL || !dir->is_open) {
        return ERR_FAIL;
    }

    if (sys_close(dir->fd) != ERR_SUCCESS) {
        return ERR_FAIL;
    }

    dir->is_open = 0;
    dir->fd = 0;
    return ERR_SUCCESS;
}

int fs_pipe(file_t* out_read_end, file_t* out_write_end) {
    if (out_read_end == NULL || out_write_end == NULL) {
        return ERR_FAIL;
    }

    uint64_t read_fd = 0;
    uint64_t write_fd = 0;

    if (sys_pipe(&read_fd, &write_fd) != ERR_SUCCESS) {
        return ERR_FAIL;
    }

    if (file_from_fd(out_read_end, read_fd) != ERR_SUCCESS) {
        return ERR_FAIL;
    }

    if (file_from_fd(out_write_end, write_fd) != ERR_SUCCESS) {
        fs_close(out_read_end);
        return ERR_FAIL;
    }

    return ERR_SUCCESS;
}

int fs_dup2(const file_t* from, file_t* to, uint64_t target_fd) {
    if (from == NULL || !from->is_open || to == NULL) {
        return ERR_FAIL;
    }

    if (sys_dup2(from->fd, target_fd) != ERR_SUCCESS) {
        return ERR_FAIL;
    }

    return file_from_fd(to, target_fd);
}

int fs_getcwd(char* buffer, size_t size) {
    if (buffer == NULL || size == 0) {
        return ERR_FAIL;
    }

    return sys_getcwd(buffer, size);
}

int fs_chdir(const char* path) {
    if (path == NULL) {
        return ERR_FAIL;
    }

    return sys_chdir(path);
}

// Backwards compat wrappers
int idp_fs_open(idp_file_t* out_file, const char* path, uint32_t flags) { return fs_open(out_file, path, flags); }
int idp_fs_read(idp_file_t* file, void* buffer, size_t bytes, size_t* out_read) { return fs_read(file, buffer, bytes, out_read); }
int idp_fs_write(idp_file_t* file, const void* buffer, size_t bytes, size_t* out_written) { return fs_write(file, buffer, bytes, out_written); }
int idp_fs_close(idp_file_t* file) { return fs_close(file); }
int idp_fs_opendir(idp_dir_t* out_dir, const char* path) { return fs_opendir(out_dir, path); }
int idp_fs_readdir(idp_dir_t* dir, idp_dir_entry_t* out_entry, int* out_has_entry) { return fs_readdir(dir, out_entry, out_has_entry); }
int idp_fs_closedir(idp_dir_t* dir) { return fs_closedir(dir); }
int idp_fs_pipe(idp_file_t* out_read_end, idp_file_t* out_write_end) { return fs_pipe(out_read_end, out_write_end); }
int idp_fs_dup2(const idp_file_t* from, idp_file_t* to, uint64_t target_fd) { return fs_dup2(from, to, target_fd); }
int idp_fs_getcwd(char* buffer, size_t size) { return fs_getcwd(buffer, size); }
int idp_fs_chdir(const char* path) { return fs_chdir(path); }
