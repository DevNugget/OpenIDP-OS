#include "gfx.h"
#include "idpimg.h"

static void gfx_memcpy8(uint8_t* dst, const uint8_t* src, uint64_t count) {
    for (uint64_t i = 0; i < count; ++i) {
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

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t headersize;
    uint32_t flags;
    uint32_t numglyph;
    uint32_t bytesperglyph;
    uint32_t height;
    uint32_t width;
} psf2_header_t;

int gfx_load_font(const char* path, gfx_font_t* out_font) {
    if (!path || !out_font) return -1;
    
    uint64_t fd = sys_open(path, IDP_O_RDONLY);
    if (fd == (uint64_t)ERR_FAIL) return -1;

    // Allocate SHM buffer for font data (256 KB max should cover most standard PSF2 fonts)
    uint64_t shm = sys_shm_create(256 * 1024);
    if ((int64_t)shm < 0) {
        sys_close(fd);
        return -1;
    }

    uint8_t* buf = (uint8_t*)sys_shm_map(shm);
    if ((uint64_t)buf == (uint64_t)-1 || buf == NULL) {
        sys_shm_destroy(shm);
        sys_close(fd);
        return -1;
    }

    size_t offset = 0;
    while (offset < 256 * 1024) {
        uint64_t rd = 0;
        if (sys_read(fd, buf + offset, 256 * 1024 - offset, &rd) != ERR_SUCCESS) {
            sys_shm_unmap(buf);
            sys_shm_destroy(shm);
            sys_close(fd);
            return -1;
        }
        if (rd == 0) break;
        offset += (size_t)rd;
    }
    sys_close(fd);

    if (offset < sizeof(psf2_header_t)) {
        sys_shm_unmap(buf);
        sys_shm_destroy(shm);
        return -1;
    }

    psf2_header_t* hdr = (psf2_header_t*)buf;
    if (hdr->magic != 0x864ab572 || hdr->headersize >= offset) {
        sys_shm_unmap(buf);
        sys_shm_destroy(shm);
        return -1;
    }

    out_font->base_buf = buf;
    out_font->shm_handle = shm;
    out_font->glyphs = buf + hdr->headersize;
    out_font->bytes_per_glyph = hdr->bytesperglyph;
    out_font->width = hdr->width;
    out_font->height = hdr->height;

    return 0;
}

void gfx_unload_font(gfx_font_t* font) {
    if (!font || !font->base_buf) return;
    sys_shm_unmap(font->base_buf);
    sys_shm_destroy(font->shm_handle);
    font->base_buf = NULL;
    font->shm_handle = 0;
}

void gfx_draw_char(gfx_context_t* ctx, const gfx_font_t* font, char c, int32_t x, int32_t y, uint32_t color) {
    if (!ctx || !ctx->back_buffer || !font || !font->glyphs) return;

    const uint8_t* g = font->glyphs + ((uint8_t)c * font->bytes_per_glyph);
    int bytes_per_row = (font->width + 7) / 8;

    for (uint32_t gy = 0; gy < font->height; gy++) {
        int32_t py = y + gy;
        if (py < 0 || (uint64_t)py >= ctx->height) continue;
        
        for (uint32_t gx = 0; gx < font->width; gx++) {
            int32_t px = x + gx;
            if (px < 0 || (uint64_t)px >= ctx->width) continue;
            
            uint8_t byte = g[gy * bytes_per_row + (gx / 8)];
            if (byte & (0x80 >> (gx % 8))) {
                ctx->back_buffer[(uint64_t)py * ctx->stride_pixels + (uint64_t)px] = color;
            }
        }
    }
}

void gfx_draw_string(gfx_context_t* ctx, const gfx_font_t* font, const char* str, int32_t x, int32_t y, uint32_t color) {
    if (!font) return;
    while (*str) {
        gfx_draw_char(ctx, font, *str, x, y, color);
        x += font->width;
        str++;
    }
}

int gfx_load_image(const char* path, gfx_image_t* out_image) {
    if (!path || !out_image) return ERR_FAIL;

    uint64_t fd = sys_open(path, IDP_O_RDONLY);
    if (fd == (uint64_t)ERR_FAIL) return ERR_FAIL;

    idpimg_header_t hdr;
    uint64_t rd = 0;
    
    if (sys_read(fd, &hdr, sizeof(idpimg_header_t), &rd) != ERR_SUCCESS || rd != sizeof(idpimg_header_t)) {
        sys_close(fd);
        return ERR_FAIL;
    }

    if (hdr.magic != IDPIMG_MAGIC) {
        sys_close(fd);
        return ERR_FAIL;
    }

    uint64_t img_size = (uint64_t)hdr.width * (uint64_t)hdr.height * sizeof(uint32_t);
    uint64_t shm = sys_shm_create(img_size);
    if ((int64_t)shm < 0) {
        sys_close(fd);
        return ERR_FAIL;
    }

    uint32_t* pixels = (uint32_t*)sys_shm_map(shm);
    if ((uint64_t)pixels == (uint64_t)-1 || pixels == NULL) {
        sys_shm_destroy(shm);
        sys_close(fd);
        return ERR_FAIL;
    }

    size_t offset = 0;
    while (offset < img_size) {
        uint64_t chunk_rd = 0;
        if (sys_read(fd, (uint8_t*)pixels + offset, img_size - offset, &chunk_rd) != ERR_SUCCESS) {
            sys_shm_unmap(pixels);
            sys_shm_destroy(shm);
            sys_close(fd);
            return ERR_FAIL;
        }
        if (chunk_rd == 0) break;
        offset += (size_t)chunk_rd;
    }
    sys_close(fd);

    out_image->width = hdr.width;
    out_image->height = hdr.height;
    out_image->pixels = pixels;
    out_image->shm_handle = shm;

    return ERR_SUCCESS;
}

void gfx_unload_image(gfx_image_t* image) {
    if (!image || !image->pixels) return;
    sys_shm_unmap(image->pixels);
    sys_shm_destroy(image->shm_handle);
    image->pixels = NULL;
    image->shm_handle = 0;
}

void gfx_draw_image(gfx_context_t* ctx, const gfx_image_t* image, int32_t x, int32_t y) {
    if (!ctx || !ctx->back_buffer || !image || !image->pixels) return;

    for (uint32_t iy = 0; iy < image->height; iy++) {
        int32_t py = y + iy;
        if (py < 0 || (uint64_t)py >= ctx->height) continue;

        for (uint32_t ix = 0; ix < image->width; ix++) {
            int32_t px = x + ix;
            if (px < 0 || (uint64_t)px >= ctx->width) continue;

            uint32_t color = image->pixels[iy * image->width + ix];
            ctx->back_buffer[(uint64_t)py * ctx->stride_pixels + (uint64_t)px] = color;
        }
    }
}