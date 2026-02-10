#ifndef IDP_IMG_H
#define IDP_IMG_H

#include "../libc/stdint.h"

#define IDPIMG_MAGIC 0x474D49

typedef struct {
    uint32_t magic;      // Verification bytes
    uint32_t width;      // Image width
    uint32_t height;     // Image height
    uint32_t flags;      // Reserved for alpha/compression flags later
    // Raw RGBA (32-bit) data follows immediately after this struct
} idpimg_header_t;

#endif