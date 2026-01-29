#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <com1.h>
#include <font_psf.h>

typedef enum {
    USE_PSF1,
    USE_PSF2
} font_type_t;

void graphics_init(void *font_file_psf1, void *font_file_psf2);

uint64_t fb_width();
uint64_t fb_height();
uint64_t fb_pitch();
uint64_t fb_bpp();

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_filled_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_char(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg, font_type_t font_type);
void fb_draw_string(const char* str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg, font_type_t font_type);

#endif