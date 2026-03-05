#ifndef LIBIDP_WINDOW_CLIENT_H
#define LIBIDP_WINDOW_CLIENT_H

#include <stdint.h>
#include <libidp/window.h>
#include <libgfx/gfx.h>

typedef struct {
    uint64_t handle;
    window_ipc_t* ipc;
    gfx_context_t gfx;
    uint64_t backbuffer_handle;
    uint32_t last_width;
    uint32_t last_height;
} idp_window_t;

int idp_window_request(const char* executable_path, const char* argument);
int idp_window_attach(uint64_t handle, idp_window_t* out_win);
void idp_window_set_title(idp_window_t* win, const char* title);
int idp_window_poll_resize(idp_window_t* win);
int idp_window_poll_key(idp_window_t* win, key_event_t* out_ev);
void idp_window_present(idp_window_t* win);
void idp_window_destroy(idp_window_t* win);

#endif