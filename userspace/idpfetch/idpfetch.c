#include "../openidp.h"
#include "../libc/stdio.h"
#include "../libc/stdint.h"

void _start(int argc, char** argv) {
	stdio_init();
    printf("\n\n");
    printf("\033]69;/images/idplogo.img\007");

    
    printf("\n\n\n\n\n\n\n\033[34mKernel\033[36m: \033[37mopenidp-dev\n");
    printf("\033[34mFramebuffer\033[36m: \033[37m1920x1080 32bpp\n");

	sys_exit(0);
}
