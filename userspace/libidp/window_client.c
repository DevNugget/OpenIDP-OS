#include <libidp/window_client.h>
#include <libidp/syscall.h>
#include <libidp/string.h>
#include <stddef.h>

#define WM_CONTROL_SCAN_MAX 65535

static void str_copy_limit(char* dst, int dst_len, const char* src) {
    if (!dst || dst_len <= 0) return;
    strlcpy(dst, src, (size_t)dst_len);
}

static uint64_t find_wm_control_handle(void) {
    for (uint64_t handle = 1; handle <= WM_CONTROL_SCAN_MAX; ++handle) {
        wm_control_ipc_t* control = (wm_control_ipc_t*)sys_shm_map(handle);
        if ((uint64_t)control == (uint64_t)-1 || control == NULL) {
            continue;
        }

        int match = (control->magic == WM_CONTROL_MAGIC) && (control->version == 1);
        sys_shm_unmap(control);
        if (match) {
            return handle;
        }
    }
    return 0;
}

int idp_window_request(const char* executable_path, const char* argument) {
    uint64_t wm_control_handle = find_wm_control_handle();
    if (wm_control_handle == 0) {
        return -1;
    }

    wm_control_ipc_t* control = (wm_control_ipc_t*)sys_shm_map(wm_control_handle);
    if ((uint64_t)control == (uint64_t)-1 || control == NULL) {
        return -1;
    }

    uint8_t next_head = (uint8_t)((control->request_head + 1) % WM_MAX_REQUESTS);
    if (next_head == control->request_tail) {
        sys_shm_unmap(control);
        return -1;
    }

    wm_spawn_request_t* req = &control->requests[control->request_head];
    str_copy_limit(req->executable, WM_PATH_MAX, executable_path);
    str_copy_limit(req->argument, WM_ARG_MAX, argument ? argument : "");
    req->pending = 1;
    control->request_head = next_head;

    sys_shm_unmap(control);
    return 0;
}

int idp_window_attach(uint64_t handle, idp_window_t* out_win) {
    if (!out_win) return -1;

    out_win->handle = handle;
    out_win->ipc = (window_ipc_t*)sys_shm_map(handle);
    if ((uint64_t)out_win->ipc == (uint64_t)-1 || out_win->ipc == NULL) {
        return -1;
    }

    out_win->backbuffer_handle = (uint64_t)sys_shm_create(WINDOW_MAX_WIDTH * WINDOW_MAX_HEIGHT * 4);
    if ((int64_t)out_win->backbuffer_handle < 0) {
        sys_shm_unmap(out_win->ipc);
        return -1;
    }

    out_win->gfx.back_buffer = (uint32_t*)sys_shm_map(out_win->backbuffer_handle);
    if ((uint64_t)out_win->gfx.back_buffer == (uint64_t)-1 || out_win->gfx.back_buffer == NULL) {
        sys_shm_destroy(out_win->backbuffer_handle);
        sys_shm_unmap(out_win->ipc);
        return -1;
    }

    out_win->gfx.front_buffer = out_win->ipc->pixels;
    out_win->gfx.width = WINDOW_MAX_WIDTH;
    out_win->gfx.height = WINDOW_MAX_HEIGHT;
    out_win->gfx.pitch_bytes = WINDOW_MAX_WIDTH * 4;
    out_win->gfx.stride_pixels = WINDOW_MAX_WIDTH;
    out_win->gfx.bpp = 32;

    out_win->last_width = 0;
    out_win->last_height = 0;

    return 0;
}

void idp_window_set_title(idp_window_t* win, const char* title) {
    if (!win || !win->ipc || !title) return;
    str_copy_limit(win->ipc->title, WINDOW_TITLE_MAX, title);
}

int idp_window_poll_resize(idp_window_t* win) {
    if (!win || !win->ipc) return 0;
    
    if (win->ipc->width != win->last_width || win->ipc->height != win->last_height) {
        win->last_width = win->ipc->width;
        win->last_height = win->ipc->height;
        
        win->gfx.width = win->last_width;
        win->gfx.height = win->last_height;
        win->gfx.pitch_bytes = win->ipc->pitch_bytes;
        win->gfx.stride_pixels = win->ipc->pitch_bytes / 4;
        return 1;
    }
    return 0;
}

int idp_window_poll_key(idp_window_t* win, key_event_t* out_ev) {
    if (!win || !win->ipc || !out_ev) return 0;

    if (win->ipc->key_tail != win->ipc->key_head) {
        *out_ev = win->ipc->key_ring[win->ipc->key_tail];
        win->ipc->key_tail = (uint8_t)((win->ipc->key_tail + 1) % IPC_MAX_KEY_EVENTS);
        return 1;
    }
    return 0;
}

void idp_window_present(idp_window_t* win) {
    if (!win || !win->ipc) return;
    gfx_present(&win->gfx);
    win->ipc->dirty = 1;
}

void idp_window_destroy(idp_window_t* win) {
    if (!win) return;
    if (win->gfx.back_buffer) {
        sys_shm_unmap(win->gfx.back_buffer);
    }
    if (win->backbuffer_handle) {
        sys_shm_destroy(win->backbuffer_handle);
    }
    if (win->ipc) {
        sys_shm_unmap(win->ipc);
    }
}