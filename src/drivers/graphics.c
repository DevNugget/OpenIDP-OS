#include <graphics.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

struct limine_framebuffer* framebuffer;
static psf1_font_t current_font_psf1;
static psf2_font_t current_font_psf2;

void graphics_init(const char* psf1_path, const char* psf2_path) {
    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        serial_printf("FATAL: No framebuffer found!\n");
        while(1); // Halt if no framebuffer
    }

    // Fetch the first framebuffer.
    framebuffer = framebuffer_request.response->framebuffers[0];

    serial_printf("Framebuffer initialized: %ux%u, %u bpp, pitch %u\n",
        (uint32_t)framebuffer->width,
        (uint32_t)framebuffer->height,
        (uint32_t)framebuffer->bpp,
        (uint32_t)framebuffer->pitch);
    
    current_font_psf1 = *psf1_load_font(psf1_path);
    current_font_psf2 = *psf2_load_font(psf2_path);
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

uint64_t fb_addr() {
    return (uint64_t)&framebuffer->address;
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    // Calculate offset: (y * pitch) + (x * bytes_per_pixel)
    uint64_t offset = (y * fb_pitch()) + (x * (fb_bpp() / 8));
    volatile uint32_t *fb_ptr = framebuffer->address;

    // Write color to that address
    fb_ptr[offset / 4] = color;
}

void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    uint32_t x_end = x + w;
    uint32_t y_end = y + h;

    for (uint32_t i = x; i < x_end; i++) {
        fb_put_pixel(i, y, color); // Top line
        fb_put_pixel(i, y_end - 1, color); // Bottom line
    }

    for (uint32_t j = y + 1; j < y_end - 1; j++) {
        fb_put_pixel(x, j, color); // Left line
        fb_put_pixel(x_end - 1, j, color); // Right line
    }
}

void fb_draw_filled_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            fb_put_pixel(i, j, color);
        }
    }
}

void fb_draw_char(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg, font_type_t font_type) {
    if (font_type == USE_PSF1) {
        if (!framebuffer || !current_font_psf1.header) return;
    
        // Get pointer to the specific character in the glyph buffer
        // PSF1 fonts are simple linear arrays of bitmaps
        uint8_t* glyph = (uint8_t*)current_font_psf1.glyph_buffer + (c * current_font_psf1.header->charsize);
    
        // Loop through the height of the character
        for (int dy = 0; dy < current_font_psf1.header->charsize; dy++) {
            // Each row of a PSF1 glyph is 1 byte (8 pixels wide)
            uint8_t row = glyph[dy];
    
            // Loop through the width (8 bits)
            for (int dx = 0; dx < 8; dx++) {
                // Check if the bit is set (from MSB to LSB)
                if ((row >> (7 - dx)) & 1) {
                    fb_put_pixel(x + dx, y + dy, fg);
                } else {
                    // If you want transparent text, remove this else block
                    fb_put_pixel(x + dx, y + dy, bg);
                }
            }
        }
    } else if (font_type == USE_PSF2) {
        if (!framebuffer || !current_font_psf2.header) return;

        psf2_header_t* h = current_font_psf2.header;
        
        // Calculate pointer to the glyph
        uint8_t* glyph = (uint8_t*)current_font_psf2.glyph_buffer + (c * h->charsize);

        // Calculate how many bytes make up a single row of pixels
        // Example: Width 8 = 1 byte. Width 16 = 2 bytes. Width 9 = 2 bytes.
        uint32_t bytes_per_row = (h->width + 7) / 8;

        for (uint32_t dy = 0; dy < h->height; dy++) {
            for (uint32_t dx = 0; dx < h->width; dx++) {
                // Calculate which byte in the row contains our bit
                uint32_t byte_offset = dx / 8;
                // Calculate which bit in that byte corresponds to x
                uint32_t bit_offset = 7 - (dx % 8);
                
                // Get the specific byte for this row
                uint8_t row_byte = glyph[dy * bytes_per_row + byte_offset];

                // Check the bit
                if ((row_byte >> bit_offset) & 1) {
                    fb_put_pixel(x + dx, y + dy, fg);
                } else {
                    fb_put_pixel(x + dx, y + dy, bg);
                }
            }
        }
    }
}

void fb_draw_string(const char* str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg, font_type_t font_type) {
    if (font_type == USE_PSF1) {
        if (!current_font_psf1.header) return;
        int cursor_x = x;
        int cursor_y = y;
    
        while (*str) {
            if (*str == '\n') {
                cursor_x = x;
                cursor_y += current_font_psf1.header->charsize;
            } else {
                fb_draw_char(*str, cursor_x, cursor_y, fg, bg, font_type);
                cursor_x += 8; // PSF1 is fixed width (8 pixels)
            }
            str++;
        }
    } else if (font_type == USE_PSF2) {
        if (!current_font_psf2.header) return;
    
        uint32_t cursor_x = x;
        uint32_t cursor_y = y;

        while (*str) {
            if (*str == '\n') {
                cursor_x = x;
                cursor_y += current_font_psf2.header->height;
            } else {
                fb_draw_char(*str, cursor_x, cursor_y, fg, bg, USE_PSF2);
                cursor_x += current_font_psf2.header->width;
            }
            str++;
        }
    }
}