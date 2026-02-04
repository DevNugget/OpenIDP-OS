#ifndef IDPWM_H
#define IDPWM_H

#include "libc/stdint.h"

#define MAX_WINDOWS 16

struct fb_info framebuffer;

// Window Structure
typedef struct {
    int id;
    int pid;          // Client Process ID
    int alive;
    int focused;
    
    // Layout (Screen Coordinates)
    int x, y;
    int width, height;

    // Shared Memory
    uint32_t* buffer_wm_ptr;     // Pointer WM uses to read
    uint64_t  buffer_client_ptr; // Pointer Client uses to write
} window_t;


#endif