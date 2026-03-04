#include <libidp/syscall.h>
#include <libidp/stdio.h>
#include <stdint.h>

static void print_u64(uint64_t value) {
    char buffer[32];
    int index = 0;

    if (value == 0) {
        putchar('0');
        return;
    }

    while (value > 0 && index < (int)sizeof(buffer)) {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        putchar(buffer[--index]);
    }
}

static void print_uptime(uint64_t uptime_ms) {
    uint64_t total_seconds = uptime_ms / 1000;
    uint64_t hours = total_seconds / 3600;
    uint64_t minutes = (total_seconds % 3600) / 60;
    uint64_t seconds = total_seconds % 60;

    print_u64(hours);
    printf("h ");
    print_u64(minutes);
    printf("m ");
    print_u64(seconds);
    printf("s");
}

static void print_memory_usage(uint64_t used_ram, uint64_t total_ram) {
    uint64_t mib = 1024 * 1024;

    print_u64(used_ram / mib);
    printf("/");
    print_u64(total_ram / mib);
    printf(" MiB");
}

static void print_header(void) {
    printf("\x1b[1;35m   ____                   ________  ____ \x1b[0m\n");
    printf("\x1b[1;35m  / __ \\____  ___  ____  /  _/ __ \\/ __ \\\x1b[0m\n");
    printf("\x1b[1;35m / / / / __ \\/ _ \\/ __ \\ / // / / / /_/ /\x1b[0m\n");
    printf("\x1b[1;35m/ /_/ / /_/ /  __/ / / // // /_/ / ____/ \x1b[0m\n");
    printf("\x1b[1;35m\\____/ .___/\\___/_/ /_/___/_____/_/      \x1b[0m\n");
    printf("\x1b[1;35m    /_/                                  \x1b[0m\n");
}

static void print_info_line(const char* key) {
    printf("\x1b[1;36m%s\x1b[0m", key);
}

void main(int argc, char** argv) {
    stdio_arginit(&argc, argv);

    sysinfo_t info;
    framebuffer_user_info_t framebuffer;

    int info_ok = (sys_info(&info) == ERR_SUCCESS);
    int framebuffer_ok = (sys_framebuffer_get_info(&framebuffer) == ERR_SUCCESS);

    print_header();

    print_info_line("OS:        ");
    printf("OpenIDP-OS\n");

    print_info_line("Kernel:    ");
    printf("idpkernel-dev\n");

    print_info_line("Shell:     ");
    printf("idpshell\n");

    print_info_line("PID:       ");
    print_u64((uint64_t)sys_getpid());
    printf("\n");

    if (info_ok) {
        print_info_line("Uptime:    ");
        print_uptime(info.uptime_ms);
        printf("\n");

        print_info_line("Memory:    ");
        print_memory_usage(info.total_ram - info.free_ram, info.total_ram);
        printf("\n");

        print_info_line("CPU Cores: ");
        print_u64((uint64_t)info.cpus);
        printf("\n");

        print_info_line("Processes: ");
        print_u64((uint64_t)info.procs);
        printf("\n");
    } else {
        print_info_line("System:    ");
        printf("unavailable\n");
    }

    if (framebuffer_ok) {
        print_info_line("Display:   ");
        print_u64(framebuffer.width);
        printf("x");
        print_u64(framebuffer.height);
        printf(" @ ");
        print_u64((uint64_t)framebuffer.bpp);
        printf("bpp\n");
    } else {
        print_info_line("Display:   ");
        printf("unavailable\n");
    }

    printf("\n");
    printf("\x1b[40m  \x1b[41m  \x1b[42m  \x1b[43m  \x1b[44m  \x1b[45m  \x1b[46m  \x1b[47m  \x1b[0m\n");

    sys_exit(0);
}
