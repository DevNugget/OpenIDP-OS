#ifndef IDPWM_H
#define IDPWM_H

#include "libc/stdint.h"

#define MAX_WINDOWS 16

typedef struct {
    int id;
    int x, y;
    int width, height;

    int focused;
    int alive;

    uint32_t *buffer;   // shared memory buffer (later)
} window_t;

#endif