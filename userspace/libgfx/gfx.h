#ifndef IDP_GFX_H
#define IDP_GFX_H

#include "../libc/stdint.h"

#define GFX_VER "libgfx "

#define PSF2_MAGIC0 0x72
#define PSF2_MAGIC1 0xB5
#define PSF2_MAGIC2 0x4A
#define PSF2_MAGIC3 0x86

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t headersize;
    uint32_t flags;
    uint32_t length;
    uint32_t charsize;
    uint32_t height;
    uint32_t width;
} psf2_header_t;

typedef struct {
    uint32_t* fb;           // Renamed from fb_buffer to match gfx.c
    int width;
    int height;
    int pitch;              // Added missing field
    
    psf2_header_t* font_header;
    uint8_t* glyph_data;    // Renamed from glyph_buffer and changed to uint8_t*
} gfx_context_t;

int gfx_init(gfx_context_t* ctx, void* font_data, uint32_t* fb, int w, int h);

void gfx_clear(gfx_context_t* ctx, uint32_t color);
void gfx_draw_char(gfx_context_t* ctx, int x, int y, char c, uint32_t fg, uint32_t bg);
void gfx_rect(gfx_context_t* ctx, int x, int y, int w, int h, uint32_t color);

int gfx_font_width(gfx_context_t* ctx);
int gfx_font_height(gfx_context_t* ctx);

void gfx_draw_image(gfx_context_t* ctx, int x, int y, void* img_data);

#endif