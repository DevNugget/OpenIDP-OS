#include <libidp/syscall.h>
#include <libidp/stdio.h>
#include <libidp/ansi.h>
#include <stddef.h>

#define MAX_LINE 256
#define MAX_TOKENS 16
#define MAX_PATH 256

static int streq(const char* a, const char* b) { while (*a && *b) { if (*a++ != *b++) return 0; } return *a == *b; }

static size_t str_len(const char* s) {
    size_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static int str_has_char(const char* s, char c) {
    while (*s) {
        if (*s++ == c) {
            return 1;
        }
    }
    return 0;
}

static int str_ends_with(const char* str, const char* suffix) {
    size_t str_len_v = str_len(str);
    size_t suffix_len = str_len(suffix);

    if (suffix_len > str_len_v) {
        return 0;
    }

    for (size_t i = 0; i < suffix_len; ++i) {
        if (str[str_len_v - suffix_len + i] != suffix[i]) {
            return 0;
        }
    }
    return 1;
}

static int copy_str(char* dst, size_t cap, const char* src) {
    if (cap == 0) {
        return -1;
    }

    size_t i = 0;
    while (src[i] != '\0') {
        if (i + 1 >= cap) {
            return -1;
        }
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return 0;
}

static int append_str(char* dst, size_t cap, const char* suffix) {
    size_t l = str_len(dst);
    return copy_str(dst + l, cap - l, suffix);
}

static int normalize_path(char* out, size_t cap, const char* cwd, const char* path) {
    if (out == NULL || cap < 2 || cwd == NULL || path == NULL) {
        return -1;
    }

    char merged[MAX_PATH];
    size_t m = 0;

    if (path[0] == '/') {
        merged[m++] = '/';
        path++;
    } else {
        for (size_t i = 0; cwd[i] != '\0'; ++i) {
            if (m + 1 >= sizeof(merged)) {
                return -1;
            }
            merged[m++] = cwd[i];
        }
        if (m == 0) {
            if (m + 1 >= sizeof(merged)) {
                return -1;
            }
            merged[m++] = '/';
        }
        if (merged[m - 1] != '/') {
            if (m + 1 >= sizeof(merged)) {
                return -1;
            }
            merged[m++] = '/';
        }
    }

    while (*path) {
        if (m + 1 >= sizeof(merged)) {
            return -1;
        }
        merged[m++] = *path++;
    }
    merged[m] = '\0';

    char result[MAX_PATH];
    size_t r = 0;
    result[r++] = '/';

    size_t i = 0;
    while (merged[i] != '\0') {
        while (merged[i] == '/') {
            i++;
        }
        if (merged[i] == '\0') {
            break;
        }

        char segment[MAX_PATH];
        size_t s = 0;
        while (merged[i] != '\0' && merged[i] != '/') {
            if (s + 1 >= sizeof(segment)) {
                return -1;
            }
            segment[s++] = merged[i++];
        }
        segment[s] = '\0';

        if (segment[0] == '.' && segment[1] == '\0') {
            continue;
        }
        if (segment[0] == '.' && segment[1] == '.' && segment[2] == '\0') {
            if (r > 1) {
                r--;
                while (r > 1 && result[r - 1] != '/') {
                    r--;
                }
            }
            continue;
        }

        if (r > 1 && result[r - 1] != '/') {
            if (r + 1 >= sizeof(result)) {
                return -1;
            }
            result[r++] = '/';
        }

        for (size_t j = 0; j < s; ++j) {
            if (r + 1 >= sizeof(result)) {
                return -1;
            }
            result[r++] = segment[j];
        }
    }

    if (r > 1 && result[r - 1] == '/') {
        r--;
    }

    result[r] = '\0';
    return copy_str(out, cap, result);
}

static int is_space_char(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int read_wm_path_from_file(char* out_path, size_t out_cap) {
    if (out_path == NULL || out_cap == 0) {
        return -1;
    }

    uint64_t fd = sys_open("/nvme/.wm", IDP_O_RDONLY);
    if (fd == (uint64_t)ERR_FAIL) {
        return -1;
    }

    char raw[MAX_PATH];
    size_t total = 0;

    while (total + 1 < sizeof(raw)) {
        uint64_t rd = 0;
        if (sys_read(fd, raw + total, sizeof(raw) - 1 - total, &rd) != ERR_SUCCESS) {
            sys_close(fd);
            return -1;
        }

        if (rd == 0) {
            break;
        }

        total += (size_t)rd;
    }

    sys_close(fd);
    raw[total] = '\0';

    size_t start = 0;
    while (raw[start] != '\0' && is_space_char(raw[start])) {
        start++;
    }

    size_t end = total;
    while (end > start && is_space_char(raw[end - 1])) {
        end--;
    }

    if (end <= start) {
        return -1;
    }

    size_t out_len = end - start;
    if (out_len + 1 > out_cap) {
        return -1;
    }

    for (size_t i = 0; i < out_len; ++i) {
        out_path[i] = raw[start + i];
    }
    out_path[out_len] = '\0';

    return 0;
}

static int tokenize(char* line, char** argv, int max) {
    int n = 0;
    while (*line && n < max) {
        while (*line == ' ') ++line;
        if (!*line) break;
        argv[n++] = line;
        while (*line && *line != ' ') ++line;
        if (*line) *line++ = '\0';
    }
    return n;
}

static uint64_t hex_to_u64(const char* str) {
    uint64_t val = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        val *= 16;
        if (str[i] >= '0' && str[i] <= '9') val += str[i] - '0';
        else if (str[i] >= 'A' && str[i] <= 'F') val += str[i] - 'A' + 10;
    }
    return val;
}

static void read_line(char* out, size_t cap) {
    size_t i = 0;
    while (i + 1 < cap) {
        int c = getchar();
        if (c == '\n') { putchar('\n'); break; }
        if (c == '\b') {
            if (i > 0) { --i; printf("\b \b"); }
            continue;
        }
        out[i++] = (char)c;
        putchar(c);
    }
    out[i] = '\0';
}

void main(int argc, char** argv) {
    char line[MAX_LINE];
    char* token_argv[MAX_TOKENS];
    char cwd[MAX_PATH] = "/";
    char resolved_path[MAX_PATH];
    char bin_path[MAX_PATH];
    char elf_path[MAX_PATH];

    int last_err_code = 0;

    while (1) {
        sys_getcwd(cwd, sizeof(cwd));
        printf(ANSI_SET_TITLE("idpshell"));
        if (last_err_code != 0) {
            printf(
                ANSI_FG_RED "err" ANSI_FG_BRIGHT_BLACK "(" ANSI_FG_WHITE "%d" ANSI_FG_BRIGHT_BLACK 
                ") :: " ANSI_FG_MAGENTA "@" ANSI_FG_BRIGHT_BLACK "[" ANSI_FG_BLUE "%s" 
                ANSI_FG_BRIGHT_BLACK "] :: " ANSI_RESET, last_err_code, cwd
            );
        } else {
            printf(
                ANSI_FG_GREEN "err" ANSI_FG_BRIGHT_BLACK "(" ANSI_FG_WHITE "%d" ANSI_FG_BRIGHT_BLACK 
                ") :: " ANSI_FG_MAGENTA "@" ANSI_FG_BRIGHT_BLACK "[" ANSI_FG_BLUE "%s" 
                ANSI_FG_BRIGHT_BLACK "] :: " ANSI_RESET, last_err_code, cwd
            );
        }

        read_line(line, sizeof(line));
        
        int token_argc = tokenize(line, token_argv, MAX_TOKENS);
        if (token_argc == 0) continue;

        if (streq(token_argv[0], "cd")) {
            const char* target = (token_argc > 1) ? token_argv[1] : "/";
            char target_path[MAX_PATH];
            
            if (normalize_path(target_path, sizeof(target_path), cwd, target) != 0) {
                puts("cd: path too long");
            } else {
                if (sys_chdir(target_path) != ERR_SUCCESS) {
                    printf("cd: %s: No such directory\n", target);
                }
            }
        } else if (streq(token_argv[0], "pwd")) {
            puts(cwd);
        } else if (streq(token_argv[0], "echo")) {
            for (int i = 1; i < token_argc; ++i) {
                printf("%s%s", token_argv[i], (i + 1 < token_argc) ? " " : "");
            }
            putchar('\n');
        } else if (streq(token_argv[0], "clear")) {
            printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME);
        } else if (streq(token_argv[0], "startwm")) {
            char wm_path[MAX_PATH];
            if (read_wm_path_from_file(wm_path, sizeof(wm_path)) != 0) {
                puts("startwm: failed to read /nvme/.wm");
                last_err_code = 1;
                continue;
            }
            printf("startwm: loading %s\n", wm_path);

            const char* wm_args[] = {wm_path, NULL};
            int wm_pid = sys_spawn(wm_path, wm_args);
            if (wm_pid < 0) {
                printf("startwm: failed to spawn %s\n", wm_path);
                last_err_code = 1;
                continue;
            }
            
            printf("\033[2;%dW", wm_pid);
            
            int code = 0;
            while (sys_wait(wm_pid, &code) == 1) {
                sys_yield();
            }
            
            sys_exit(0);
        } else if (streq(token_argv[0], "exit")) {
            sys_exit(0);
        } else {
            if (str_has_char(token_argv[0], '/')) {
                if (normalize_path(resolved_path, sizeof(resolved_path), cwd, token_argv[0]) != 0) {
                    puts("idpshell: command path too long");
                    continue;
                }
            } else {
                if (str_len(cwd) + 1 + str_len(token_argv[0]) + 1 > sizeof(resolved_path)) {
                    puts("idpshell: command path too long");
                    continue;
                }

                if (copy_str(resolved_path, sizeof(resolved_path), cwd) != 0) {
                    puts("idpshell: command path too long");
                    continue;
                }

                size_t l = str_len(resolved_path);
                if (l == 0 || resolved_path[l - 1] != '/') {
                    resolved_path[l++] = '/';
                    resolved_path[l] = '\0';
                }
                if (copy_str(resolved_path + l, sizeof(resolved_path) - l, token_argv[0]) != 0) {
                    puts("idpshell: command path too long");
                    continue;
                }
            }

            int has_bin_fallback = 0;
            int has_elf_fallback = 0;
            if (copy_str(bin_path, sizeof(bin_path), "/nvme/bin/") == 0) {
                size_t bin_len = str_len(bin_path);
                if (copy_str(bin_path + bin_len, sizeof(bin_path) - bin_len, token_argv[0]) == 0) {
                    has_bin_fallback = 1;
                }
            }

            if (!str_ends_with(token_argv[0], ".elf") && copy_str(elf_path, sizeof(elf_path), "/nvme/bin/") == 0) {
                size_t elf_len = str_len(elf_path);
                if (copy_str(elf_path + elf_len, sizeof(elf_path) - elf_len, token_argv[0]) == 0 &&
                    append_str(elf_path, sizeof(elf_path), ".elf") == 0) {
                    has_elf_fallback = 1;
                }
            }

            if (token_argc + 1 >= MAX_TOKENS) {
                puts("idpshell: too many arguments");
                continue;
            }

            token_argv[token_argc] = NULL;

            int pid = sys_spawn(resolved_path, (const char**)token_argv);
            if (pid < 0 && has_bin_fallback && !str_has_char(token_argv[0], '/')) {
                pid = sys_spawn(bin_path, (const char**)token_argv);
            }
            if (pid < 0 && has_elf_fallback && !str_has_char(token_argv[0], '/')) {
                pid = sys_spawn(elf_path, (const char**)token_argv);
            }
            if (pid < 0) {
                printf("idpshell: command not found: %s\n", token_argv[0]);
                continue;
            }
            
            int code = 0;
            while (sys_wait(pid, &code) == 1) {
                sys_yield();
            }
            
            last_err_code = code;
        }
    }
}