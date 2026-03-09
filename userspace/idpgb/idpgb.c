#include "libidp/syscall.h"
#include <libidp/window_client.h>
#include <libidp/stdio.h>
#include <libgfx/gfx.h>

void main() {
    idp_window_t window;
    if (idp_window_open(&window, "idpgba") != ERR_SUCCESS) {
        printf("idpgb: Error, could not create window.\n");
        sys_exit(1);
    }

    int running = 1;
    while (running) {
        key_event_t ev;
        while (idp_window_poll_key(&window, &ev)) {
            if (ev.is_pressed && ev.code == KEY_ESC) {
                running = 0;
            }
        }

        gfx_clear(&window.gfx, gfx_rgb(30, 30, 46));
        gfx_fill_rect(&window.gfx, 100, 100, 200, 150, gfx_rgb(180, 190, 254));

        idp_window_present(&window);
        sys_yield();
    }

    idp_window_destroy(&window);
    sys_exit(0);
}
