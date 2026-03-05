#include <libidp/syscall.h>
#include <libidp/stdio.h>

// TODO app

void main(int argc, char** argv) {
    printf("I received %d real arguments from the shell:\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("  arg[%d]: %s\n", i, argv[i]);
    }

    sys_exit(0);
}
