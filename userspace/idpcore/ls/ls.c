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

void main(int argc, char** argv) {
    const char* path = "/nvme";
    int long_format = 0;

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
        printf("Listing %s:\n", path);
    }

    while (idp_fs_readdir(&dir, &entry, &has_entry) == ERR_SUCCESS) {
        if (!has_entry) {
            break;
        }

        if (entry.name[0] == '.') {
            continue;
        }

        if (long_format) {
            if (entry.type == IDP_DIRENT_TYPE_DIR) {
                printf(ANSI_FG_YELLOW"DIR "ANSI_FG_BLUE"%s/\n", entry.name);
            } else {
                printf(ANSI_FG_YELLOW"FIL "ANSI_FG_WHITE"%s\n", entry.name);
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
