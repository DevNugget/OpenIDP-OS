#include <libidp/stdio.h>
#include <libidp/syscall.h>

#define STDIN_FD  0
#define STDOUT_FD 1
#define STDERR_FD 2

#define STDOUT_BUF_SIZE 4096
static char g_stdout_buf[STDOUT_BUF_SIZE];
static int g_stdout_len = 0;

void fflush(void) {
    if (g_stdout_len > 0) {
        uint64_t wr = 0;
        sys_write(STDOUT_FD, g_stdout_buf, (uint64_t)g_stdout_len, &wr);
        g_stdout_len = 0;
    }
}

int putchar(int c) {
    g_stdout_buf[g_stdout_len++] = (char)c;
    if (g_stdout_len >= STDOUT_BUF_SIZE || c == '\n') {
        fflush();
    }
    return c;
}

int getchar(void) {
    fflush();
    char ch;
    uint64_t rd = 0;
    while (rd == 0) {
        if (sys_read(STDIN_FD, &ch, 1, &rd) != 0) { 
            return -1; 
        }
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
