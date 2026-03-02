#include <libidp/syscall.h>

static inline uint32_t make_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void _start(void) {
    framebuffer_user_info_t fb;
    if (sys_framebuffer_get_info(&fb) != 0) {
        sys_print("idpwm: failed to get framebuffer info\n");
        sys_exit(1);
    }

    if (fb.bpp != 32) {
        sys_print("idpwm: only 32bpp framebuffers are currently supported\n");
        sys_exit(2);
    }

    volatile uint32_t* pixels = (volatile uint32_t*)sys_shm_map(fb.shm_handle);
    if ((uint64_t)pixels == (uint64_t)-1) {
        sys_print("idpwm: failed to map framebuffer shared memory\n");
        sys_exit(3);
    }

    uint64_t stride = fb.pitch / 4;
    for (uint64_t y = 0; y < fb.height; ++y) {
        for (uint64_t x = 0; x < fb.width; ++x) {
            uint8_t r = (uint8_t)((x * 255) / (fb.width ? fb.width : 1));
            uint8_t g = (uint8_t)((y * 255) / (fb.height ? fb.height : 1));
            uint8_t b = 0x40;
            pixels[y * stride + x] = make_rgb(r, g, b);
        }
    }

    sys_print("idpwm: framebuffer gradient rendered via shared memory\n");

    for (;;) {
        sys_yield();
    }
}
