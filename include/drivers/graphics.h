#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <com1.h>

void graphics_init();

uint64_t fb_width();
uint64_t fb_height();
uint64_t fb_pitch();
uint64_t fb_bpp();
uint64_t fb_size();

#endif
