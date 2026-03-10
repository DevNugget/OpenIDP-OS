#include <emulator.h>
#include <cartridge.h>
#include <LR35902.h>

#include <libidp/syscall.h>
#include <libidp/window_client.h>
#include <libidp/stdio.h>
#include <libgfx/gfx.h>

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
    printf("idpgb: cartridge %s loaded.\n", argv[1]);

    idp_window_t window;
    if (idp_window_open(&window, "idpgba") != ERR_SUCCESS) {
        printf("idpgb: error, could not create window.\n");
        sys_exit(1);
    }
    printf("idpgb: window created.\n");

    lr35902_init();
    emulator_ctx.running = true;
    emulator_ctx.paused = false;
    emulator_ctx.ticks = 0;

    while (emulator_ctx.running) {
        key_event_t ev;
        while (idp_window_poll_key(&window, &ev)) {
            if (ev.is_pressed && ev.code == KEY_ESC) {
                emulator_ctx.running = 0;
            }
        }

        if (!lr35902_step()) {
            printf("idpgb: cpu stopped.\n");
            sys_exit(-3);
        }

        emulator_ctx.ticks++;

        gfx_clear(&window.gfx, gfx_rgb(30, 30, 46));
        gfx_fill_rect(&window.gfx, 100, 100, 200, 150, gfx_rgb(180, 190, 254));

        idp_window_present(&window);
        sys_yield();
    }

    idp_window_destroy(&window);
}

emulator_ctx_t* get_emulator_ctx() {
    return &emulator_ctx;
}

void emulator_cycles(int cpu_cycles) {

}