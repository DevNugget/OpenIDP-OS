#include <libidp/stdio.h>
#include <libidp/syscall.h>

void main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: cat <filename>\n");
        sys_exit(1);
    }

    uint64_t fd = sys_open(argv[1], IDP_O_RDONLY);
    
    if (fd == (uint64_t)ERR_FAIL) {
        printf("cat: could not open %s\n", argv[1]);
        sys_exit(1);
    }

    char buffer[512];
    uint64_t bytes_read = 0;

    while (sys_read(fd, buffer, sizeof(buffer), &bytes_read) == ERR_SUCCESS) {
        if (bytes_read == 0) break;

        for (uint64_t i = 0; i < bytes_read; i++) {
            putchar(buffer[i]);
        }
    }

    sys_close(fd);
    sys_exit(0);
}