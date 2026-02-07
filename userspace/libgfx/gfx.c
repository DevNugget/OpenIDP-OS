#include "gfx.h"
#include "../libc/string.h"

// Helper to check PSF2 magic
static int is_valid_psf2(void* buffer) {
    uint8_t* b = (uint8_t*)buffer;
    return (b[0] == PSF2_MAGIC0 && b[1] == PSF2_MAGIC1 && 
            b[2] == PSF2_MAGIC2 && b[3] == PSF2_MAGIC3);
}

int gfx_init(gfx_context_t* ctx, void* font_buffer, uint32_t* fb_addr, int width, int height) {
    if (!ctx || !font_buffer || !fb_addr) return -1;

    if (!is_valid_psf2(font_buffer)) {
        return -1; // Invalid Magic
    }

    ctx->fb = fb_addr;
    ctx->width = width;
    ctx->height = height;
    ctx->pitch = width; // Assuming linear pitch for now

    ctx->font_header = (psf2_header_t*)font_buffer;
    // Glyph data starts immediately after the header
    ctx->glyph_data = (uint8_t*)font_buffer + ctx->font_header->headersize;

    return 0;
}

int gfx_font_width(gfx_context_t* ctx) {
    return ctx->font_header ? ctx->font_header->width : 8;
}

int gfx_font_height(gfx_context_t* ctx) {
    return ctx->font_header ? ctx->font_header->height : 16;
}

void gfx_clear(gfx_context_t* ctx, uint32_t color) {
    // TODO: Refactor to use memset 
    int size = ctx->width * ctx->height;
    for (int i = 0; i < size; i++) {
        ctx->fb[i] = color;
    }
}

void gfx_rect(gfx_context_t* ctx, int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ctx->width) w = ctx->width - x;
    if (y + h > ctx->height) h = ctx->height - y;
    
    if (w <= 0 || h <= 0) return;

    for (int row = 0; row < h; row++) {
        uint32_t* row_ptr = ctx->fb + ((y + row) * ctx->pitch) + x;
        for (int col = 0; col < w; col++) {
            row_ptr[col] = color;
        }
    }
}

void gfx_draw_char(gfx_context_t* ctx, int x, int y, char c, uint32_t fg, uint32_t bg) {
    if (!ctx->font_header) return;
    
    // Don't draw off-screen
    if (x >= ctx->width || y >= ctx->height) return;

    int char_w = ctx->font_header->width;
    int char_h = ctx->font_header->height;
    
    // Calculate pointer to this specific glyph
    // c * charsize gives us the offset in bytes for this char
    uint8_t* glyph = ctx->glyph_data + (c * ctx->font_header->charsize);

    // Bytes per row calculation: (width + 7) / 8
    int bytes_per_row = (char_w + 7) / 8;

    for (int row = 0; row < char_h; row++) {
        // Stop if we go off bottom of screen
        if (y + row >= ctx->height) break;
        
        uint32_t* pixel_ptr = ctx->fb + ((y + row) * ctx->pitch) + x;

        for (int col = 0; col < char_w; col++) {
            // Stop if we go off right of screen
            if (x + col >= ctx->width) break;

            // PSF2 Logic:
            // Find which byte contains the bit for this pixel
            int byte_offset = col / 8;
            // Find which bit (MSB first: 7, 6, 5...)
            int bit_offset = 7 - (col % 8);
            
            // Get the byte from the glyph data
            uint8_t byte = glyph[row * bytes_per_row + byte_offset];
            
            // Extract the bit
            int is_set = (byte >> bit_offset) & 1;

            // Draw FG or BG
            pixel_ptr[col] = is_set ? fg : bg;
        }
    }
}
