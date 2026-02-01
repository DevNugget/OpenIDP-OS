#include "idpwm.h"
#include "openidp.h"

struct fb_info framebuffer;

static window_t windows[MAX_WINDOWS];
static int window_count = 0;
static int focused_window = -1;

void fb_put_pixel(uint32_t* fb_ptr, int x, int y, uint32_t color) {
    if ((unsigned)x >= framebuffer.fb_width || (unsigned)y >= framebuffer.fb_height)
        return;

    fb_ptr[y * framebuffer.fb_pitch/4 + x] = color;
}

void fb_fill_rect(uint32_t* fb_ptr, int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0)
        return;

    for (int yy = 0; yy < h; yy++) {
        int py = y + yy;
        if ((unsigned)py >= framebuffer.fb_height)
            continue;

        uint32_t *row = fb_ptr + py * framebuffer.fb_pitch/4;

        for (int xx = 0; xx < w; xx++) {
            int px = x + xx;
            if ((unsigned)px >= framebuffer.fb_width)
                continue;

            row[px] = color;
        }
    }
}

void _start() {
    if (sys_get_framebuffer_info(&framebuffer) != 0) {
        sys_exit(1);
    }

    uint32_t* fb_ptr = (uint32_t*)framebuffer.fb_addr;
    fb_fill_rect(fb_ptr, 10, 10, 50, 50, 0x00ffffff);

    sys_exit(0);
}