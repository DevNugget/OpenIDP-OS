#include <font_psf.h>

psf1_font_t* psf1_load_font(void* font_file) {
    psf1_header_t* header = (psf1_header_t*)font_file;

    // Verify PSF1 magic numbers
    if (header->magic[0] != PSF1_MAGIC0 || header->magic[1] != PSF1_MAGIC1) {
        serial_printf("PSF_FONT_LOADER: Invalid PSF1 Font Magic!\n");
        return NULL;
    }

    psf1_font_t* font = (psf1_font_t*)kmalloc(sizeof(psf1_font_t));
    font->header = header;

    // Calculate glyph buffer location
    font->glyph_buffer = (void*)((uint64_t)font_file + sizeof(psf1_header_t));

    return font;
}

psf2_font_t* psf2_load_font(void* font_file) {
    psf2_header_t* header = (psf2_header_t*)font_file;

    // Verify PSF2 magic numbers
    if (header->magic != (PSF2_MAGIC0 | (PSF2_MAGIC1 << 8) | (PSF2_MAGIC2 << 16) | (PSF2_MAGIC3 << 24))) {
        serial_printf("PSF_FONT_LOADER: Invalid PSF2 Font Magic!\n");
        return NULL;
    }

    psf2_font_t* font = (psf2_font_t*)kmalloc(sizeof(psf2_font_t));
    font->header = header;

    // Calculate glyph buffer location
    font->glyph_buffer = (void*)((uint64_t)font_file + header->headersize);

    return font;
}