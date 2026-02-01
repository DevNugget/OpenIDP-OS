#ifndef OPENIDP_H
#define OPENIDP_H

#include "libc/stdint.h"

#define SYS_WRITE 0
#define SYS_EXIT  1
#define SYS_GET_FB_INFO 8

struct fb_info {
    uint64_t fb_addr;
    uint64_t fb_width;
    uint64_t fb_height;
    uint64_t fb_pitch;
    uint64_t fb_bpp;
};

static inline int sys_write(int fd, const char* buf) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYS_WRITE), "D" ((uint64_t)fd), "S" ((uint64_t)buf)
        : "memory"
    );

    return ret;
}

static inline void sys_exit(int code) {
    int ret;
        asm volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYS_EXIT), "D" ((uint64_t)code)
        : "memory"
    );
}

static inline int sys_get_framebuffer_info(struct fb_info* out) {
    int ret;

    asm volatile (
    "int $0x80"
    : "=a" (ret)
    : "a" (SYS_GET_FB_INFO), "D" (out)
    : "memory"
    );
    return ret;
}

#endif