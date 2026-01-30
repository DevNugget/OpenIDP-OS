#include <font_psf.h>

psf1_font_t* psf1_load_font(const char* path) {
    FIL file;
    FRESULT res;
    UINT bytes_read;

    // 1. Open the file using FatFs
    res = f_open(&file, path, FA_READ);
    if (res != FR_OK) {
        serial_printf("PSF1_LOADER: Could not open file %s (Error: %d)\n", path, res);
        return NULL;
    }

    // 2. Get file size and allocate memory for the whole file
    uint64_t file_size = f_size(&file);
    void* buffer = kmalloc(file_size);
    if (!buffer) {
        serial_printf("PSF1_LOADER: Out of memory loading %s\n", path);
        f_close(&file);
        return NULL;
    }

    // 3. Read the file into the buffer
    res = f_read(&file, buffer, file_size, &bytes_read);
    f_close(&file); // Close file handle

    if (res != FR_OK || bytes_read != file_size) {
        serial_printf("PSF1_LOADER: Failed to read file %s\n", path);
        kfree(buffer);
        return NULL;
    }

    // 4. Parse as PSF1
    psf1_header_t* header = (psf1_header_t*)buffer;

    if (header->magic[0] != PSF1_MAGIC0 || header->magic[1] != PSF1_MAGIC1) {
        serial_printf("PSF1_LOADER: Invalid PSF1 Font Magic in %s\n", path);
        kfree(buffer);
        return NULL;
    }

    psf1_font_t* font = (psf1_font_t*)kmalloc(sizeof(psf1_font_t));
    font->header = header;
    font->glyph_buffer = (void*)((uint64_t)buffer + sizeof(psf1_header_t));

    serial_printf("PSF1_LOADER: Loaded %s successfully.\n", path);
    return font;
}

psf2_font_t* psf2_load_font(const char* path) {
    FIL file;
    FRESULT res;
    UINT bytes_read;

    // 1. Open the file
    res = f_open(&file, path, FA_READ);
    if (res != FR_OK) {
        serial_printf("PSF2_LOADER: Could not open file %s (Error: %d)\n", path, res);
        return NULL;
    }

    // 2. Allocate memory
    uint64_t file_size = f_size(&file);
    void* buffer = kmalloc(file_size);
    if (!buffer) {
        serial_printf("PSF2_LOADER: Out of memory loading %s\n", path);
        f_close(&file);
        return NULL;
    }

    // 3. Read file
    res = f_read(&file, buffer, file_size, &bytes_read);
    f_close(&file);

    if (res != FR_OK || bytes_read != file_size) {
        serial_printf("PSF2_LOADER: Failed to read file %s\n", path);
        kfree(buffer);
        return NULL;
    }

    // 4. Verify PSF2 magic numbers
    psf2_header_t* header = (psf2_header_t*)buffer;
    
    // Note: Assuming Little Endian (x86_64)
    if (header->magic != (PSF2_MAGIC0 | (PSF2_MAGIC1 << 8) | (PSF2_MAGIC2 << 16) | (PSF2_MAGIC3 << 24))) {
        serial_printf("PSF2_LOADER: Invalid PSF2 Font Magic in %s\n", path);
        kfree(buffer);
        return NULL;
    }

    psf2_font_t* font = (psf2_font_t*)kmalloc(sizeof(psf2_font_t));
    font->header = header;
    font->glyph_buffer = (void*)((uint64_t)buffer + header->headersize);

    serial_printf("PSF2_LOADER: Loaded %s successfully. (%dx%d)\n", path, header->width, header->height);
    return font;
}