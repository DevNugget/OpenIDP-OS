#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>

typedef struct framebuffer_user_info_t {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint64_t size_bytes;
    uint64_t shm_handle;
} framebuffer_user_info_t;

int framebuffer_init_shared_memory(void);
int framebuffer_get_user_info(framebuffer_user_info_t* out_info);

#endif
