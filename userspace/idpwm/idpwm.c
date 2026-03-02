#include <libidp/syscall.h>
#include <libgfx/gfx.h>

static void render_demo(gfx_context_t* gfx) {
    gfx_clear(gfx, gfx_rgb(12, 16, 28));

    uint64_t panel_w = gfx->width / 2;
    uint64_t panel_h = gfx->height / 2;
    int32_t panel_x = (int32_t)((gfx->width - panel_w) / 2);
    int32_t panel_y = (int32_t)((gfx->height - panel_h) / 2);

    gfx_fill_rect(
        gfx, panel_x, panel_y, (int32_t)panel_w, 
        (int32_t)panel_h, gfx_rgb(24, 31, 52)
    );
    gfx_draw_rect(
        gfx, panel_x, panel_y, (int32_t)panel_w, 
        (int32_t)panel_h, gfx_rgb(90, 132, 255)
    );

    gfx_draw_line(
        gfx, panel_x, panel_y,
        panel_x + (int32_t)panel_w - 1,
        panel_y + (int32_t)panel_h - 1,
        gfx_rgb(220, 80, 90)
    );

    gfx_draw_line(
        gfx, panel_x + (int32_t)panel_w - 1, panel_y,
        panel_x, panel_y + (int32_t)panel_h - 1,
        gfx_rgb(60, 220, 120)
    );

    for (uint64_t x = 0; x < gfx->width; ++x) {
        uint8_t shade = (uint8_t)((x * 255) / (gfx->width ? gfx->width : 1));
        gfx_put_pixel(gfx, (int32_t)x, 0, gfx_rgb(shade, shade, 255));
    }
}

void main() {
    gfx_context_t gfx = {0};

    if (gfx_init(&gfx) != ERR_SUCCESS) {
        sys_print("[IDPWM] libgfx initialization failed!\n");
        sys_exit(1);
    }

    render_demo(&gfx);
    gfx_present(&gfx);

    for (;;) {
        sys_yield();
    }
}