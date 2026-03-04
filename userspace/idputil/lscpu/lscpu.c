#include <libidp/syscall.h>
#include <libidp/stdio.h>

void main(int argc, char** argv) {
    if (argc < 3) sys_exit(1);
    stdio_arginit(&argc, argv);
    
    printf("I received %d real arguments from the shell:\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("  arg[%d]: %s\n", i, argv[i]);
    }

    sys_exit(0);
}
