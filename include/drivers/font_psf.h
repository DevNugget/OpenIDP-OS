#ifndef FONT_PSF_H
#define FONT_PSF_H

#include <stdint.h>
#include <stddef.h>
#include <kheap.h>
#include <com1.h>

// PSF1 Header Magic Constants
#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

typedef struct {
    uint8_t magic[2];
    uint8_t mode;
    uint8_t charsize; // Height of the character (usually 16)
} psf1_header_t;

typedef struct {
    psf1_header_t* header;
    void* glyph_buffer;
} psf1_font_t;

// PSF2 Magic: 0x864ab572 (Little Endian)
#define PSF2_MAGIC0 0x72
#define PSF2_MAGIC1 0xB5
#define PSF2_MAGIC2 0x4A
#define PSF2_MAGIC3 0x86

typedef struct {
    uint32_t magic;         // Magic bytes
    uint32_t version;       // Zero
    uint32_t headersize;    // Offset of bitmaps in file
    uint32_t flags;         // 0 if there's no unicode table
    uint32_t length;        // Number of glyphs
    uint32_t charsize;      // Size of each glyph in bytes
    uint32_t height;        // Height in pixels
    uint32_t width;         // Width in pixels
} psf2_header_t;

typedef struct {
    psf2_header_t* header;
    void* glyph_buffer;
} psf2_font_t;

psf1_font_t* psf1_load_font(void* font_file);
psf2_font_t* psf2_load_font(void* font_file);

#endif