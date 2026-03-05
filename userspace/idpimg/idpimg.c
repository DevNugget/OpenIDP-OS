#include <libidp/syscall.h>
#include <libidp/window.h>
#include <libidp/stdio.h>
#include <libgfx/gfx.h>
#include <stdint.h>
#include <stddef.h>

#define WM_CONTROL_SCAN_MAX 65535

static void str_copy(char* dst, int dst_len, const char* src) {
    if (!dst || dst_len <= 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }

    int i = 0;
    while (src[i] && i < dst_len - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int starts_with_number(const char* s) {
    return s && s[0] >= '0' && s[0] <= '9';
}

static uint64_t parse_u64(const char* arg) {
    uint64_t value = 0;
    if (!arg) return 0;

    for (int i = 0; arg[i] != '\0'; ++i) {
        if (arg[i] < '0' || arg[i] > '9') break;
        value = (value * 10) + (uint64_t)(arg[i] - '0');
    }
    return value;
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

static int request_wm_window(const char* image_path) {
    uint64_t wm_control_handle = find_wm_control_handle();
    if (wm_control_handle == 0) {
        sys_print("idpimg: idpwm control channel not found\n");
        return -1;
    }

    wm_control_ipc_t* control = (wm_control_ipc_t*)sys_shm_map(wm_control_handle);
    if ((uint64_t)control == (uint64_t)-1 || control == NULL) {
        sys_print("idpimg: failed to map idpwm control channel\n");
        return -1;
    }

    uint8_t next_head = (uint8_t)((control->request_head + 1) % WM_MAX_REQUESTS);
    if (next_head == control->request_tail) {
        sys_shm_unmap(control);
        sys_print("idpimg: idpwm request queue is full\n");
        return -1;
    }

    wm_spawn_request_t* req = &control->requests[control->request_head];
    str_copy(req->executable, WM_PATH_MAX, "/nvme/bin/idpimg.elf");
    str_copy(req->argument, WM_ARG_MAX, image_path);
    req->pending = 1;
    control->request_head = next_head;

    sys_shm_unmap(control);
    return 0;
}

static int init_gfx_from_window(window_ipc_t* ipc, gfx_context_t* gfx, uint64_t* out_bb_handle) {
    *out_bb_handle = (uint64_t)sys_shm_create(WINDOW_MAX_WIDTH * WINDOW_MAX_HEIGHT * 4);
    if ((int64_t)*out_bb_handle < 0) {
        return -1;
    }

    gfx->back_buffer = (uint32_t*)sys_shm_map(*out_bb_handle);
    if ((uint64_t)gfx->back_buffer == (uint64_t)-1 || gfx->back_buffer == NULL) {
        sys_shm_destroy(*out_bb_handle);
        *out_bb_handle = 0;
        return -1;
    }

    gfx->front_buffer = ipc->pixels;
    gfx->width = WINDOW_MAX_WIDTH;
    gfx->height = WINDOW_MAX_HEIGHT;
    gfx->pitch_bytes = WINDOW_MAX_WIDTH * 4;
    gfx->stride_pixels = WINDOW_MAX_WIDTH;
    gfx->bpp = 32;

    return 0;
}

static void set_window_title(window_ipc_t* ipc, const char* image_path) {
    const char* prefix = " idpimg - ";
    int idx = 0;

    while (prefix[idx] && idx < WINDOW_TITLE_MAX - 1) {
        ipc->title[idx] = prefix[idx];
        idx++;
    }

    int src_i = 0;
    while (image_path && image_path[src_i] && idx < WINDOW_TITLE_MAX - 1) {
        ipc->title[idx++] = image_path[src_i++];
    }

    ipc->title[idx] = '\0';
}

static void draw_image_fit(gfx_context_t* gfx, const gfx_image_t* image, uint32_t view_w, uint32_t view_h) {
    if (view_w == 0 || view_h == 0 || image->width == 0 || image->height == 0) {
        return;
    }

    gfx_fill_rect(gfx, 0, 0, (int32_t)view_w, (int32_t)view_h, gfx_rgb(20, 23, 34));

    uint64_t scale_x_num = view_w;
    uint64_t scale_x_den = image->width;
    uint64_t scale_y_num = view_h;
    uint64_t scale_y_den = image->height;

    uint64_t draw_w = view_w;
    uint64_t draw_h = (uint64_t)image->height * scale_x_num / scale_x_den;

    if (draw_h > view_h) {
        draw_h = view_h;
        draw_w = (uint64_t)image->width * scale_y_num / scale_y_den;
    }

    if (draw_w == 0) draw_w = 1;
    if (draw_h == 0) draw_h = 1;

    int32_t x_off = (int32_t)((view_w - draw_w) / 2);
    int32_t y_off = (int32_t)((view_h - draw_h) / 2);

    for (uint32_t y = 0; y < draw_h; ++y) {
        uint32_t src_y = (uint32_t)(((uint64_t)y * image->height) / draw_h);
        for (uint32_t x = 0; x < draw_w; ++x) {
            uint32_t src_x = (uint32_t)(((uint64_t)x * image->width) / draw_w);
            uint32_t color = image->pixels[(uint64_t)src_y * image->width + src_x];
            gfx_put_pixel(gfx, x_off + (int32_t)x, y_off + (int32_t)y, color);
        }
    }
}

void main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: idpimg.elf <path/to/.idpimg>\n");
        sys_exit(1);
    }

    if (!starts_with_number(argv[1])) {
        // BUGFIX 1: Verify the file actually exists BEFORE asking the WM for a window!
        uint64_t test_fd = sys_open(argv[1], IDP_O_RDONLY);
        if (test_fd == (uint64_t)ERR_FAIL) {
            printf("idpimg: cannot open file or file does not exist: %s\n", argv[1]);
            sys_exit(1);
        }
        sys_close(test_fd); // File exists, close it and proceed.

        if (request_wm_window(argv[1]) != 0) {
            sys_exit(1);
        }
        sys_exit(0);
    }

    if (argc < 3 || argv[2] == NULL) {
        printf("idpimg: missing image path after window handle\n");
        sys_exit(1);
    }

    uint64_t window_handle = parse_u64(argv[1]);
    const char* image_path = argv[2];
    
    // BUGFIX 2: Do this *before* assigning any IPC or backbuffer handles to prevent freeing invalid memory on failure
    gfx_image_t image = {0};
    if (gfx_load_image(image_path, &image) != ERR_SUCCESS) {
        printf("idpimg: failed to load image file\n");
        sys_exit(1); 
    }

    window_ipc_t* ipc = (window_ipc_t*)sys_shm_map(window_handle);
    if ((uint64_t)ipc == (uint64_t)-1 || ipc == NULL) {
        printf("idpimg: failed to map window shared memory\n");
        gfx_unload_image(&image);
        sys_exit(1);
    }

    gfx_context_t gfx = {0};
    uint64_t backbuffer_handle = 0;
    if (init_gfx_from_window(ipc, &gfx, &backbuffer_handle) != 0) {
        sys_shm_unmap(ipc);
        gfx_unload_image(&image);
        sys_exit(1);
    }

    set_window_title(ipc, image_path);

    uint32_t last_w = 0;
    uint32_t last_h = 0;
    uint8_t running = 1;

    while (running) {
        // BUGFIX 3: Removed `|| ipc->dirty == 0`. We only want to redraw if the window size actually changed.
        if (ipc->width != last_w || ipc->height != last_h) {
            last_w = ipc->width;
            last_h = ipc->height;

            gfx.width = last_w;
            gfx.height = last_h;
            gfx.pitch_bytes = ipc->pitch_bytes;
            gfx.stride_pixels = ipc->pitch_bytes / 4;

            draw_image_fit(&gfx, &image, last_w, last_h);
            gfx_present(&gfx);
            ipc->dirty = 1;
        }

        while (ipc->key_tail != ipc->key_head) {
            key_event_t ev = ipc->key_ring[ipc->key_tail];
            ipc->key_tail = (uint8_t)((ipc->key_tail + 1) % IPC_MAX_KEY_EVENTS);
            if (ev.is_pressed && (ev.code == KEY_ESC || ev.code == KEY_Q)) {
                running = 0;
                break;
            }
        }

        sys_yield();
    }

    gfx_unload_image(&image);
    sys_shm_unmap(gfx.back_buffer);
    sys_shm_destroy(backbuffer_handle);
    sys_shm_unmap(ipc);
    sys_exit(0);
}