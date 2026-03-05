#include <libidp/syscall.h>
#include <libidp/window_client.h>
#include <libidp/stdio.h>
#include <libgfx/gfx.h>
#include <stdint.h>
#include <stddef.h>

#define WM_CONTROL_SCAN_MAX 65535

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
        uint64_t test_fd = sys_open(argv[1], IDP_O_RDONLY);
        if (test_fd == (uint64_t)ERR_FAIL) {
            printf("idpimg: cannot open file or file does not exist: %s\n", argv[1]);
            sys_exit(1);
        }
        sys_close(test_fd);

        if (idp_window_request("/nvme/bin/idpimg.elf", argv[1]) != 0) {
            printf("idpimg: idpwm control channel not found or full\n");
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
    
    gfx_image_t image = {0};
    if (gfx_load_image(image_path, &image) != ERR_SUCCESS) {
        printf("idpimg: failed to load image file\n");
        sys_exit(1); 
    }

    idp_window_t win;
    if (idp_window_attach(window_handle, &win) != 0) {
        printf("idpimg: failed to initialize window\n");
        gfx_unload_image(&image);
        sys_exit(1);
    }

    char title_buf[WINDOW_TITLE_MAX];
    const char* prefix = " idpimg - ";
    int idx = 0, src_idx = 0;
    while (prefix[idx] && idx < WINDOW_TITLE_MAX - 1) { title_buf[idx] = prefix[idx]; idx++; }
    while (image_path[src_idx] && idx < WINDOW_TITLE_MAX - 1) { title_buf[idx++] = image_path[src_idx++]; }
    title_buf[idx] = '\0';
    idp_window_set_title(&win, title_buf);

    uint8_t running = 1;

    while (running) {
        if (idp_window_poll_resize(&win)) {
            draw_image_fit(&win.gfx, &image, win.gfx.width, win.gfx.height);
            idp_window_present(&win);
        }

        key_event_t ev;
        while (idp_window_poll_key(&win, &ev)) {
            if (ev.is_pressed && (ev.code == KEY_ESC || ev.code == KEY_Q)) {
                running = 0;
                break;
            }
        }

        sys_yield();
    }

    gfx_unload_image(&image);
    idp_window_destroy(&win);
    sys_exit(0);
}