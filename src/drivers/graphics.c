#include <graphics.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

struct limine_framebuffer* framebuffer;

void graphics_init() {
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        serial_printf("FATAL: No framebuffer found!\n");
        while(1); 
    }

    framebuffer = framebuffer_request.response->framebuffers[0];

    serial_printf("Framebuffer initialized: %ux%u, %u bpp, pitch %u\n",
        (uint32_t)framebuffer->width,
        (uint32_t)framebuffer->height,
        (uint32_t)framebuffer->bpp,
        (uint32_t)framebuffer->pitch);
}

uint64_t fb_width() {
    return framebuffer->width;
}

uint64_t fb_height() {
    return framebuffer->height;
}

uint64_t fb_pitch() {
    return framebuffer->pitch;
}

uint64_t fb_bpp() {
    return framebuffer->bpp;
}

uint64_t fb_size() {
    return fb_pitch() * fb_height();
}
