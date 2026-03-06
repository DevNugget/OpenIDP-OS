#include <libidp/stdio.h>
#include <libidp/ansi.h>
#include <libidp/fs.h>
#include <libidp/syscall.h>

static int streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

// Helper function to build the full path to a file
static void build_path(char* dest, const char* dir, const char* file) {
    int i = 0;
    while (dir[i]) {
        dest[i] = dir[i];
        i++;
    }
    // Add trailing slash if necessary
    if (i > 0 && dest[i-1] != '/') {
        dest[i++] = '/';
    }
    int j = 0;
    while (file[j]) {
        dest[i++] = file[j++];
    }
    dest[i] = '\0';
}

// Because there is no SYS_STAT syscall, we find the size by reading the file
static int get_file_size(const char* full_path) {
    idp_file_t file;
    if (idp_fs_open(&file, full_path, IDP_O_RDONLY) != ERR_SUCCESS) {
        return 0;
    }
    
    int total_size = 0;
    char buf[512];
    size_t read_bytes = 0;
    
    // Read the file in chunks until EOF to count the bytes
    while (idp_fs_read(&file, buf, sizeof(buf), &read_bytes) == ERR_SUCCESS) {
        if (read_bytes == 0) {
            break;
        }
        total_size += read_bytes;
    }
    
    idp_fs_close(&file);
    return total_size;
}

// Helper to extract extension and convert to uppercase
static void get_extension(const char* name, char* ext_out) {
    int len = 0;
    while (name[len]) len++;
    
    int dot_idx = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (name[i] == '.') {
            dot_idx = i;
            break;
        }
    }
    
    if (dot_idx > 0 && dot_idx < len - 1) {
        int j = 0;
        for (int i = dot_idx + 1; name[i] && j < 3; i++) {
            char c = name[i];
            if (c >= 'a' && c <= 'z') c -= 32; // Convert to uppercase
            ext_out[j++] = c;
        }
        // Pad with spaces to keep alignment
        while (j < 3) ext_out[j++] = ' ';
        ext_out[3] = '\0';
    } else {
        // Fallback if no extension
        ext_out[0] = 'F'; 
        ext_out[1] = 'I'; 
        ext_out[2] = 'L'; 
        ext_out[3] = '\0';
    }
}

void main(int argc, char** argv) {
    char current_dir[256];
    const char* path = "/nvme";
    int long_format = 0;

    if (sys_getcwd(current_dir, sizeof(current_dir)) == ERR_SUCCESS) {
        path = current_dir;
    }

    for (int i = 1; i < argc; ++i) {
        if (streq(argv[i], "-l")) {
            long_format = 1;
        } else {
            path = argv[i];
        }
    }

    idp_dir_t dir;
    if (idp_fs_opendir(&dir, path) != ERR_SUCCESS) {
        printf("ls: cannot open directory %s\n", path);
        sys_exit(1);
    }

    idp_dir_entry_t entry;
    int has_entry = 0;

    if (long_format) {
        printf("Listing "ANSI_FG_MAGENTA"%s"ANSI_RESET":\n", path);
    }

    while (idp_fs_readdir(&dir, &entry, &has_entry) == ERR_SUCCESS) {
        if (!has_entry) {
            break;
        }

        if (entry.name[0] == '.') {
            continue;
        }

        if (long_format) {
            char full_path[256];
            build_path(full_path, path, entry.name);
            
            if (entry.type == IDP_DIRENT_TYPE_DIR) {
                printf(ANSI_FG_YELLOW"DIR "ANSI_FG_BLUE"%s/ "ANSI_FG_WHITE"\n", entry.name);
            } else {
                char ext[4];
                get_extension(entry.name, ext);
                int size = get_file_size(full_path);
                printf(ANSI_FG_YELLOW"%s "ANSI_FG_WHITE"%s ", ext, entry.name);
                if (size >= 1024 * 1024 * 1024) {
                    printf("("ANSI_FG_BRIGHT_WHITE"%dG"ANSI_FG_WHITE")\n", size / (1024 * 1024 * 1024));
                } else if (size >= 1024 * 1024) {
                    printf("("ANSI_FG_BRIGHT_WHITE"%dM"ANSI_FG_WHITE")\n", size / (1024 * 1024));
                } else if (size >= 1024) {
                    printf("("ANSI_FG_BRIGHT_WHITE"%dK"ANSI_FG_WHITE")\n", size / 1024);
                } else {
                    printf("("ANSI_FG_BRIGHT_WHITE"%d"ANSI_FG_WHITE")\n", size);
                }
            }
        } else {
            if (entry.type == IDP_DIRENT_TYPE_DIR) {
                printf(ANSI_FG_BLUE"%s/\n", entry.name);
            } else {
                printf(ANSI_FG_WHITE"%s\n", entry.name);
            }
        }
    }

    idp_fs_closedir(&dir);
    sys_exit(0);
}