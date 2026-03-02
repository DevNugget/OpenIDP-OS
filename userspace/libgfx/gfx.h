#ifndef LIBGFX_GFX_H
#define LIBGFX_GFX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <libidp/syscall.h>

typedef struct gfx_context_t {
    uint32_t* front_buffer;
    uint32_t* back_buffer;
    uint64_t width;
    uint64_t height;
    uint64_t pitch_bytes;
    uint64_t stride_pixels;
    uint16_t bpp;
    uint64_t framebuffer_shm_handle;
    uint64_t back_buffer_shm_handle;
    bool owns_back_buffer;
} gfx_context_t;

int gfx_init(gfx_context_t* ctx);
void gfx_shutdown(gfx_context_t* ctx);

void gfx_present(gfx_context_t* ctx);
void gfx_clear(gfx_context_t* ctx, uint32_t color);

void gfx_put_pixel(gfx_context_t* ctx, int32_t x, int32_t y, uint32_t color);
void gfx_draw_line(gfx_context_t* ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void gfx_draw_rect(gfx_context_t* ctx, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t color);
void gfx_fill_rect(gfx_context_t* ctx, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t color);

static inline uint32_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

#endif
