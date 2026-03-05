#ifndef LIBGFX_IDPIMG_H
#define LIBGFX_IDPIMG_H

#include <stdint.h>

#define IDPIMG_MAGIC 0x49504449

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t width;
    uint32_t height;
} idpimg_header_t;

#endif