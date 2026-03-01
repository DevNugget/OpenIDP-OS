#include <libidp/syscall.h>

void main(void) {
    sys_print("Hello from userspace\n");
    sys_print("Test test, test test.\n");

    sys_exit(0);
}
