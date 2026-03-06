#include <libidp/fs.h>

int idp_fs_open(idp_file_t* out_file, const char* path, uint32_t flags) {
    if (out_file == NULL || path == NULL) {
        return ERR_FAIL;
    }

    uint64_t fd = sys_open(path, flags);
    if (fd == (uint64_t)ERR_FAIL) {
        return ERR_FAIL;
    }

    out_file->fd = fd;
    out_file->is_open = 1;
    return ERR_SUCCESS;
}

int idp_fs_read(idp_file_t* file, void* buffer, size_t bytes, size_t* out_read) {
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

int idp_fs_close(idp_file_t* file) {
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

int idp_fs_opendir(idp_dir_t* out_dir, const char* path) {
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

int idp_fs_readdir(idp_dir_t* dir, idp_dir_entry_t* out_entry, int* out_has_entry) {
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

int idp_fs_closedir(idp_dir_t* dir) {
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
