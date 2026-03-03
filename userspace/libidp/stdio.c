#include <libidp/stdio.h>
#include <libidp/syscall.h>

static uint64_t g_stdin = 0;
static uint64_t g_stdout = 0;

static uint64_t hex_to_u64(const char* str) {
    uint64_t val = 0;
    if (!str) return 0;
    for (int i = 0; str[i] != '\0'; i++) {
        val *= 16;
        if (str[i] >= '0' && str[i] <= '9') val += (uint64_t)(str[i] - '0');
        else if (str[i] >= 'A' && str[i] <= 'F') val += (uint64_t)(str[i] - 'A' + 10);
        else if (str[i] >= 'a' && str[i] <= 'f') val += (uint64_t)(str[i] - 'a' + 10);
    }
    return val;
}

void stdio_init(uint64_t stdin_fd, uint64_t stdout_fd) {
    g_stdin = stdin_fd;
    g_stdout = stdout_fd;
}

void stdio_arginit(int* argc, char** argv) {
    if (*argc < 3) return; 

    uint64_t stdin_fd = hex_to_u64(argv[*argc - 2]);
    uint64_t stdout_fd = hex_to_u64(argv[*argc - 1]);
    stdio_init(stdin_fd, stdout_fd);

    *argc -= 2;
    argv[*argc] = NULL; 
}

int putchar(int c) {
    uint64_t wr = 0;
    char ch = (char)c;
    sys_write(g_stdout, &ch, 1, &wr); 
    return c;
}

int getchar(void) {
    char ch;
    uint64_t rd = 0;
    while (rd == 0) {
        sys_read(g_stdin, &ch, 1, &rd);
        if (rd == 0) sys_yield();
    }
    return (int)ch;
}

int puts(const char* s) {
    while (*s) {
        putchar(*s++);
    }
    putchar('\n');
    return 0;
}

static void print_int(int val) {
    if (val == 0) { putchar('0'); return; }
    if (val < 0) { putchar('-'); val = -val; }
    char buf[32];
    int i = 30;
    buf[31] = '\0';
    while (val > 0 && i >= 0) {
        buf[i--] = (char)((val % 10) + '0');
        val /= 10;
    }
    const char* str = &buf[i + 1];
    while (*str) putchar(*str++);
}

static void print_hex(unsigned int val) {
    if (val == 0) { putchar('0'); return; }
    const char* hex = "0123456789abcdef";
    char buf[16];
    int i = 15;
    while (val > 0 && i >= 0) {
        buf[i--] = hex[val % 16];
        val /= 16;
    }
    for (int j = i + 1; j <= 15; j++) putchar(buf[j]);
}

int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    while (*format) {
        if (*format == '%') {
            format++;
            if (*format == 's') {
                const char* str = va_arg(args, const char*);
                while (*str) putchar(*str++);
            } else if (*format == 'd') {
                print_int(va_arg(args, int));
            } else if (*format == 'x') {
                print_hex(va_arg(args, unsigned int));
            } else if (*format == 'c') {
                putchar(va_arg(args, int));
            } else if (*format == '%') {
                putchar('%');
            }
        } else {
            putchar(*format);
        }
        format++;
    }
    
    va_end(args);
    return 0;
}