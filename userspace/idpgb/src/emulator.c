#include <emulator.h>
#include <cartridge.h>
#include <libidp/syscall.h>
#include <libidp/stdio.h>

static emulator_ctx_t emulator_ctx;

int emulator_run(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: idpgb <path/to/rom>\n");
        sys_exit(-1);
    }

    if (!cartridge_load(argv[1])) {
        printf("idpgb: failed to load rom file: %s\n", argv[1]);
        sys_exit(-2);
    }
}

emulator_ctx_t* get_emulator_ctx() {
    return &emulator_ctx;
}