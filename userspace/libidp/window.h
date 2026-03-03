#ifndef LIBIDP_WINDOW_H
#define LIBIDP_WINDOW_H

#include <stdint.h>
#include <libidp/syscall.h>

#define IPC_MAX_KEY_EVENTS 16
#define WINDOW_MAX_WIDTH 1920
#define WINDOW_MAX_HEIGHT 1080

#define WINDOW_TITLE_MAX 128

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch_bytes;

    key_event_t key_ring[IPC_MAX_KEY_EVENTS];
    uint8_t key_head;
    uint8_t key_tail;

    char title[WINDOW_TITLE_MAX];
    volatile uint8_t dirty;

    uint32_t pixels[]; 
} window_ipc_t;

#endif