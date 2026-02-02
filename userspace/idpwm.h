#ifndef IDPWM_H
#define IDPWM_H

#include "libc/stdint.h"

#define MAX_WINDOWS 16

// --- Protocol Definitions ---
// Messages FROM Client
#define MSG_REQUEST_WINDOW 100 

// Messages TO Client
#define MSG_WINDOW_CREATED 200 // d1=w, d2=h, d3=buffer_ptr
#define MSG_WINDOW_RESIZE  201 // d1=w, d2=h

// Common
#define MSG_BUFFER_UPDATE  300

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