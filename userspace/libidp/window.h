#ifndef LIBIDP_WINDOW_H
#define LIBIDP_WINDOW_H

#include <stdint.h>
#include <libidp/syscall.h>

#define IPC_MAX_KEY_EVENTS 16
#define WINDOW_MAX_WIDTH 1920
#define WINDOW_MAX_HEIGHT 1080

#define WINDOW_TITLE_MAX 128

#define WM_CONTROL_MAGIC 0x574D4354u
#define WM_MAX_REQUESTS 16
#define WM_PATH_MAX 256
#define WM_ARG_MAX 256

typedef struct {
    volatile uint8_t pending;
    char executable[WM_PATH_MAX];
    char argument[WM_ARG_MAX];
} wm_spawn_request_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    volatile uint8_t request_head;
    volatile uint8_t request_tail;
    wm_spawn_request_t requests[WM_MAX_REQUESTS];
} wm_control_ipc_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch_bytes;

    key_event_t key_ring[IPC_MAX_KEY_EVENTS];
    uint8_t key_head;
    uint8_t key_tail;

    char title[WINDOW_TITLE_MAX];
    volatile uint8_t dirty;
    volatile uint8_t focused;

    uint32_t pixels[]; 
} window_ipc_t;

#endif