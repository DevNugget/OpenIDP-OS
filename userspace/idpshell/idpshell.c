#include <libidp/syscall.h>
#include <libidp/stdio.h>
#include <stddef.h>

#define MAX_LINE 256
#define MAX_TOKENS 16

static int streq(const char* a, const char* b) { while (*a && *b) { if (*a++ != *b++) return 0; } return *a == *b; }

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

    while (1) {
        printf("\x1b]0;idpshell\x07");
        printf("\x1b[35midpshell :: \x1b[0m");
        read_line(line, sizeof(line));
        
        int token_argc = tokenize(line, token_argv, MAX_TOKENS);
        if (token_argc == 0) continue;

        if (streq(token_argv[0], "cd")) {
            puts("cd: not yet implemented");
        } else if (streq(token_argv[0], "pwd")) {
            puts("/");
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
            token_argv[token_argc++] = argv[1];
            token_argv[token_argc++] = argv[2];
            token_argv[token_argc] = NULL;
            int pid = sys_spawn(token_argv[0], (const char**)token_argv);
            if (pid < 0) {
                printf("idpshell: command not found: %s\n", token_argv[0]);
                continue;
            }
            
            int code = 0;
            while (sys_wait(pid, &code) == 1) {
                sys_yield();
            }
            
            if (code != 0) {
                printf("idpshell: process %d exited with code %d\n", pid, code);
            }
        }
    }
}