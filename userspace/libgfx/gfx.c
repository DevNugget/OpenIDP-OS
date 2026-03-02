#include "gfx.h"

static void gfx_memset32(uint32_t* dst, uint32_t value, uint64_t count) {
    for (uint64_t i = 0; i < count; ++i) {
        dst[i] = value;
    }
}

static void gfx_memcpy8(uint8_t* dst, const uint8_t* src, uint64_t count) {
    for (uint64_t i = 0; i < count; ++i) {
        dst[i] = src[i];
    }
}

static void gfx_memcpy32(uint32_t* dst, const uint32_t* src, uint64_t count_u32) {
    for (uint64_t i = 0; i < count_u32; ++i) {
        dst[i] = src[i];
    }
}

static int32_t gfx_abs_i32(int32_t value) {
    return (value < 0) ? -value : value;
}

static void gfx_swap_i32(int32_t* a, int32_t* b) {
    int32_t t = *a;
    *a = *b;
    *b = t;
}

int gfx_init(gfx_context_t* ctx) {
    framebuffer_user_info_t fb;

    if (ctx == NULL) {
        return ERR_FAIL;
    }

    if (sys_framebuffer_get_info(&fb) != ERR_SUCCESS) {
        return ERR_FAIL;
    }

    if (fb.bpp != 32) {
        return ERR_FAIL;
    }

    uint32_t* front = (uint32_t*)sys_shm_map(fb.shm_handle);
    if ((uint64_t)front == (uint64_t)-1) {
        return ERR_FAIL;
    }

    uint64_t back_size = fb.pitch * fb.height;
    int64_t back_handle = sys_shm_create(back_size);
    if (back_handle < 0) {
        sys_shm_unmap(front);
        return ERR_FAIL;
    }

    uint32_t* back = (uint32_t*)sys_shm_map((uint64_t)back_handle);
    if ((uint64_t)back == (uint64_t)-1) {
        sys_shm_destroy((uint64_t)back_handle);
        sys_shm_unmap(front);
        return ERR_FAIL;
    }

    ctx->front_buffer = front;
    ctx->back_buffer = back;
    ctx->width = fb.width;
    ctx->height = fb.height;
    ctx->pitch_bytes = fb.pitch;
    ctx->stride_pixels = fb.pitch / 4;
    ctx->bpp = fb.bpp;
    ctx->framebuffer_shm_handle = fb.shm_handle;
    ctx->back_buffer_shm_handle = (uint64_t)back_handle;
    ctx->owns_back_buffer = true;

    return ERR_SUCCESS;
}

void gfx_shutdown(gfx_context_t* ctx) {
    if (ctx == NULL) {
        return;
    }

    if (ctx->back_buffer != NULL) {
        sys_shm_unmap(ctx->back_buffer);
    }

    if (ctx->owns_back_buffer && ctx->back_buffer_shm_handle != 0) {
        sys_shm_destroy(ctx->back_buffer_shm_handle);
    }

    if (ctx->front_buffer != NULL) {
        sys_shm_unmap(ctx->front_buffer);
    }

    ctx->front_buffer = NULL;
    ctx->back_buffer = NULL;
    ctx->width = 0;
    ctx->height = 0;
    ctx->pitch_bytes = 0;
    ctx->stride_pixels = 0;
    ctx->bpp = 0;
    ctx->framebuffer_shm_handle = 0;
    ctx->back_buffer_shm_handle = 0;
    ctx->owns_back_buffer = false;
}

void gfx_present(gfx_context_t* ctx) {
    if (ctx == NULL || ctx->front_buffer == NULL || ctx->back_buffer == NULL) {
        return;
    }

    uint32_t* dst = ctx->front_buffer;
    const uint32_t* src = ctx->back_buffer;
    uint64_t words_per_frame = (ctx->pitch_bytes * ctx->height) / 4;
    gfx_memcpy32(dst, src, words_per_frame);
}

void gfx_clear(gfx_context_t* ctx, uint32_t color) {
    if (ctx == NULL || ctx->back_buffer == NULL) {
        return;
    }

    uint64_t pixels = ctx->stride_pixels * ctx->height;
    gfx_memset32(ctx->back_buffer, color, pixels);
}

void gfx_put_pixel(gfx_context_t* ctx, int32_t x, int32_t y, uint32_t color) {
    if (ctx == NULL || ctx->back_buffer == NULL) {
        return;
    }

    if (x < 0 || y < 0) {
        return;
    }

    if ((uint64_t)x >= ctx->width || (uint64_t)y >= ctx->height) {
        return;
    }

    ctx->back_buffer[(uint64_t)y * ctx->stride_pixels + (uint64_t)x] = color;
}

void gfx_draw_line(gfx_context_t* ctx, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    int32_t dx = gfx_abs_i32(x1 - x0);
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t dy = -gfx_abs_i32(y1 - y0);
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;

    for (;;) {
        gfx_put_pixel(ctx, x0, y0, color);

        if (x0 == x1 && y0 == y1) {
            break;
        }

        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_draw_rect(gfx_context_t* ctx, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t color) {
    if (width <= 0 || height <= 0) {
        return;
    }

    gfx_draw_line(ctx, x, y, x + width - 1, y, color);
    gfx_draw_line(ctx, x, y, x, y + height - 1, color);
    gfx_draw_line(ctx, x + width - 1, y, x + width - 1, y + height - 1, color);
    gfx_draw_line(ctx, x, y + height - 1, x + width - 1, y + height - 1, color);
}

void gfx_fill_rect(gfx_context_t* ctx, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t color) {
    if (ctx == NULL || ctx->back_buffer == NULL || width <= 0 || height <= 0) {
        return;
    }

    if (x > x + width - 1 || y > y + height - 1) {
        return;
    }

    int32_t x0 = x;
    int32_t y0 = y;
    int32_t x1 = x + width - 1;
    int32_t y1 = y + height - 1;

    if (x0 > x1) {
        gfx_swap_i32(&x0, &x1);
    }
    if (y0 > y1) {
        gfx_swap_i32(&y0, &y1);
    }

    if (x1 < 0 || y1 < 0) {
        return;
    }

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }

    if ((uint64_t)x0 >= ctx->width || (uint64_t)y0 >= ctx->height) {
        return;
    }

    if ((uint64_t)x1 >= ctx->width) {
        x1 = (int32_t)(ctx->width - 1);
    }
    if ((uint64_t)y1 >= ctx->height) {
        y1 = (int32_t)(ctx->height - 1);
    }

    uint64_t span = (uint64_t)(x1 - x0 + 1);
    for (int32_t py = y0; py <= y1; ++py) {
        uint32_t* row = &ctx->back_buffer[(uint64_t)py * ctx->stride_pixels + (uint64_t)x0];
        gfx_memset32(row, color, span);
    }
}
