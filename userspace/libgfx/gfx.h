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

typedef struct gfx_font_t {
    uint8_t* base_buf;
    uint64_t shm_handle;
    const uint8_t* glyphs;
    uint32_t bytes_per_glyph;
    uint32_t width;
    uint32_t height;
} gfx_font_t;

typedef struct gfx_image_t {
    uint32_t width;
    uint32_t height;
    uint32_t* pixels;
    uint64_t shm_handle;
} gfx_image_t;

inline void gfx_memset32(uint32_t* dst, uint32_t value, uint64_t count) {
    __asm__ volatile (
        "rep stosl"
        : "+D" (dst), "+c" (count)
        : "a" (value)
        : "memory"
    );
}

inline void gfx_memcpy32(uint32_t* dst, const uint32_t* src, uint64_t count_u32) {
    __asm__ volatile (
        "rep movsl"
        : "+D" (dst), "+S" (src), "+c" (count_u32)
        :
        : "memory"
    );
}

int gfx_init(gfx_context_t* ctx);
void gfx_shutdown(gfx_context_t* ctx);

void gfx_present(gfx_context_t* ctx);
void gfx_clear(gfx_context_t* ctx, uint32_t color);

void gfx_put_pixel(gfx_context_t* ctx, int32_t x, int32_t y, uint32_t color);
void gfx_draw_line(gfx_context_t* ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void gfx_draw_rect(gfx_context_t* ctx, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t color);
void gfx_fill_rect(gfx_context_t* ctx, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t color);

int gfx_load_font(const char* path, gfx_font_t* out_font);
void gfx_unload_font(gfx_font_t* font);
void gfx_draw_char(gfx_context_t* ctx, const gfx_font_t* font, char c, int32_t x, int32_t y, uint32_t color);
void gfx_draw_string(gfx_context_t* ctx, const gfx_font_t* font, const char* str, int32_t x, int32_t y, uint32_t color);

int gfx_load_image(const char* path, gfx_image_t* out_image);
void gfx_unload_image(gfx_image_t* image);
void gfx_draw_image(gfx_context_t* ctx, const gfx_image_t* image, int32_t x, int32_t y);

static inline uint32_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

#endif
