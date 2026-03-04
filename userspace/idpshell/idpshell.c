#include <libidp/syscall.h>
#include <libidp/stdio.h>
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
    if (argc < 3) sys_exit(1);

    uint64_t stdin_fd = hex_to_u64(argv[1]);
    uint64_t stdout_fd = hex_to_u64(argv[2]);
    stdio_init(stdin_fd, stdout_fd);

    char line[MAX_LINE];
    char* token_argv[MAX_TOKENS];
    char cwd[MAX_PATH] = "/";
    char resolved_path[MAX_PATH];
    char bin_path[MAX_PATH];

    int last_err_code = 0;

    while (1) {
        printf("\x1b]0;idpshell\x07");
        if (last_err_code != 0) {
            printf("\x1b[31merr\x1b[90m(\x1b[37m%d\x1b[90m) :: \x1b[35m@\x1b[90m[\x1b[34m%s\x1b[90m] :: \x1b[0m", last_err_code, cwd);
        } else {
            printf("\x1b[32merr\x1b[90m(\x1b[37m%d\x1b[90m) :: \x1b[35m@\x1b[90m[\x1b[34m%s\x1b[90m] :: \x1b[0m", last_err_code, cwd);
        }
        read_line(line, sizeof(line));
        
        int token_argc = tokenize(line, token_argv, MAX_TOKENS);
        if (token_argc == 0) continue;

        if (streq(token_argv[0], "cd")) {
            const char* target = (token_argc > 1) ? token_argv[1] : "/";
            if (normalize_path(cwd, sizeof(cwd), cwd, target) != 0) {
                puts("cd: path too long");
            }
        } else if (streq(token_argv[0], "pwd")) {
            puts(cwd);
        } else if (streq(token_argv[0], "echo")) {
            for (int i = 1; i < token_argc; ++i) {
                printf("%s%s", token_argv[i], (i + 1 < token_argc) ? " " : "");
            }
            putchar('\n');
        } else if (streq(token_argv[0], "clear")) {
            printf("\x1b[2J\x1b[H"); 
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
            if (copy_str(bin_path, sizeof(bin_path), "/nvme/bin/") == 0) {
                size_t bin_len = str_len(bin_path);
                if (copy_str(bin_path + bin_len, sizeof(bin_path) - bin_len, token_argv[0]) == 0) {
                    has_bin_fallback = 1;
                }
            }

            if (token_argc + 2 >= MAX_TOKENS) {
                puts("idpshell: too many arguments");
                continue;
            }

            token_argv[token_argc++] = argv[1];
            token_argv[token_argc++] = argv[2];
            token_argv[token_argc] = NULL;

            int pid = sys_spawn(resolved_path, (const char**)token_argv);
            if (pid < 0 && has_bin_fallback && !str_has_char(token_argv[0], '/')) {
                pid = sys_spawn(bin_path, (const char**)token_argv);
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