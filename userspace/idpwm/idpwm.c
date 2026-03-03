#include <libidp/syscall.h>
#include <libidp/window.h>
#include <libgfx/gfx.h>
#include <stdint.h>
#include <stdbool.h>

#define IDPWM_MAX_CLIENTS 8
#define IDPWM_GAP_OUTER 10
#define IDPWM_GAP_INNER 8
#define IDPWM_BORDER_WIDTH 2

typedef enum {
    LAYOUT_MASTER_STACK = 0,
    LAYOUT_MONOCLE = 1
} wm_layout_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} rect_t;

typedef struct {
    uint8_t id;
    int32_t pid;
    uint8_t alive;
    uint32_t color;
    rect_t frame;
    uint64_t shm_handle;
    window_ipc_t* ipc;
} wm_client_t;

typedef struct {
    gfx_context_t* gfx;
    wm_client_t clients[IDPWM_MAX_CLIENTS];
    uint8_t client_count;
    uint8_t focused_index;
    wm_layout_t layout;
    uint8_t running;
    uint8_t master_ratio_percent;
    uint8_t dirty;
} wm_state_t;

gfx_font_t g_title_font;

static uint32_t palette[] = {
    0x181825, // base
    0xb4befe, // lavender
    0x6c7086, // overlay 0
    0x45475a, // surface 2
    0x2A9D8F, 0xE76F51, 0x457B9D, 0xF4A261,
    0x8D99AE, 0xB56576, 0x5E60CE, 0x6A994E
};

static uint32_t shade(uint32_t color, uint8_t amount) {
    uint8_t r = (uint8_t)((color >> 16) & 0xFF);
    uint8_t g = (uint8_t)((color >> 8) & 0xFF);
    uint8_t b = (uint8_t)(color & 0xFF);

    uint8_t add = amount;
    if (r + add < r) r = 255; else r = (uint8_t)(r + add);
    if (g + add < g) g = 255; else g = (uint8_t)(g + add);
    if (b + add < b) b = 255; else b = (uint8_t)(b + add);

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void draw_client(wm_state_t* wm, uint8_t idx) {
    wm_client_t* c = &wm->clients[idx];
    uint8_t focused = idx == wm->focused_index;
    uint32_t border = focused ? palette[1] : palette[2];

    int32_t padding = 4;
    int32_t tb_h = (g_title_font.height > 0 ? g_title_font.height : 10) + padding;
    gfx_fill_rect(wm->gfx, c->frame.x, c->frame.y, c->frame.w, c->frame.h, palette[0]);
    gfx_fill_rect(wm->gfx, c->frame.x, c->frame.y, c->frame.w, tb_h, palette[3]);

    for (int32_t i = 0; i < IDPWM_BORDER_WIDTH; ++i) {
        gfx_draw_rect(wm->gfx, c->frame.x + i, c->frame.y + i, c->frame.w - (i * 2), c->frame.h - (i * 2), border);
    }

    int text_x = c->frame.x + IDPWM_BORDER_WIDTH + 4;
    int text_y = c->frame.y + (padding * 1.5);
    if (c->ipc && c->ipc->title[0] != '\0') {
        gfx_draw_string(wm->gfx, &g_title_font, c->ipc->title, text_x, text_y, palette[0]);
    } else {
        gfx_draw_string(wm->gfx, &g_title_font, "Window", text_x, text_y, palette[0]);
    }

    int32_t cx = c->frame.x + IDPWM_BORDER_WIDTH;
    int32_t cy = c->frame.y + tb_h;
    int32_t cw = c->frame.w - (IDPWM_BORDER_WIDTH * 2);
    int32_t ch = c->frame.h - tb_h - IDPWM_BORDER_WIDTH;

    if (cw <= 0 || ch <= 0 || !c->ipc) return;

    c->ipc->width = (uint32_t)cw;
    c->ipc->height = (uint32_t)ch;

    for (int32_t y = 0; y < ch; y++) {
        uint32_t* dst_row = &wm->gfx->back_buffer[(cy + y) * wm->gfx->stride_pixels + cx];
        const uint32_t* src_row = &c->ipc->pixels[y * WINDOW_MAX_WIDTH];
        
        gfx_memcpy32(dst_row, src_row, cw);
    }
}

static void layout_master_stack(wm_state_t* wm) {
    uint8_t n = wm->client_count;
    int32_t sw = (int32_t)wm->gfx->width;
    int32_t sh = (int32_t)wm->gfx->height;

    int32_t ox = IDPWM_GAP_OUTER;
    int32_t oy = IDPWM_GAP_OUTER;
    int32_t ww = sw - (IDPWM_GAP_OUTER * 2);
    int32_t wh = sh - (IDPWM_GAP_OUTER * 2);

    if (n == 0 || ww <= 0 || wh <= 0) {
        return;
    }

    if (n == 1) {
        wm->clients[0].frame = (rect_t){ox, oy, ww, wh};
        return;
    }

    int32_t master_w = (ww * (int32_t)wm->master_ratio_percent) / 100;
    if (master_w < 100) master_w = 100;
    if (master_w > ww - 100) master_w = ww - 100;

    int32_t stack_x = ox + master_w + IDPWM_GAP_INNER;
    int32_t stack_w = ww - master_w - IDPWM_GAP_INNER;

    wm->clients[0].frame = (rect_t){ox, oy, master_w, wh};

    uint8_t stack_count = n - 1;
    int32_t total_gaps = IDPWM_GAP_INNER * (stack_count - 1);
    int32_t stack_h_available = wh - total_gaps;
    int32_t each_h = stack_h_available / stack_count;

    int32_t y = oy;
    for (uint8_t i = 0; i < stack_count; ++i) {
        int32_t h = each_h;
        if (i == stack_count - 1) {
            h = oy + wh - y;
        }

        wm->clients[i + 1].frame = (rect_t){stack_x, y, stack_w, h};
        y += h + IDPWM_GAP_INNER;
    }
}

static void layout_monocle(wm_state_t* wm) {
    int32_t ox = IDPWM_GAP_OUTER;
    int32_t oy = IDPWM_GAP_OUTER;
    int32_t ww = (int32_t)wm->gfx->width - (IDPWM_GAP_OUTER * 2);
    int32_t wh = (int32_t)wm->gfx->height - (IDPWM_GAP_OUTER * 2);

    for (uint8_t i = 0; i < wm->client_count; ++i) {
        wm->clients[i].frame = (rect_t){ox, oy, ww, wh};
    }
}

static void wm_arrange(wm_state_t* wm) {
    if (wm->layout == LAYOUT_MONOCLE) {
        layout_monocle(wm);
    } else {
        layout_master_stack(wm);
    }
}

static void wm_render(wm_state_t* wm) {
    gfx_clear(wm->gfx, gfx_rgb(15, 18, 27));

    gfx_fill_rect(wm->gfx, 0, 0, (int32_t)wm->gfx->width, 22, gfx_rgb(24, 30, 44));
    gfx_draw_line(wm->gfx, 0, 22, (int32_t)wm->gfx->width - 1, 22, gfx_rgb(66, 86, 120));

    if (wm->layout == LAYOUT_MONOCLE && wm->client_count > 0) {
        draw_client(wm, wm->focused_index);
    } else {
        for (uint8_t i = 0; i < wm->client_count; ++i) {
            draw_client(wm, i);
        }
    }

    gfx_present(wm->gfx);
}

static void wm_focus_next(wm_state_t* wm) {
    if (wm->client_count == 0) return;
    wm->focused_index = (uint8_t)((wm->focused_index + 1) % wm->client_count);
}

static void wm_focus_prev(wm_state_t* wm) {
    if (wm->client_count == 0) return;
    if (wm->focused_index == 0) {
        wm->focused_index = (uint8_t)(wm->client_count - 1);
    } else {
        wm->focused_index--;
    }
}

static void wm_spawn_terminal_client(wm_state_t* wm) {
    if (wm->client_count >= IDPWM_MAX_CLIENTS) return;

    uint8_t idx = wm->client_count;
    wm_client_t* c = &wm->clients[idx];
    c->id = idx + 1;
    c->alive = 1;
    c->color = palette[idx % (sizeof(palette) / sizeof(palette[0]))];
    c->frame = (rect_t){0, 0, 0, 0};

    uint64_t shm_size = sizeof(window_ipc_t) + (WINDOW_MAX_WIDTH * WINDOW_MAX_HEIGHT * 4);
    c->shm_handle = (uint64_t)sys_shm_create(shm_size);
    if ((int64_t)c->shm_handle < 0) {
        return;
    }
    c->ipc = (window_ipc_t*)sys_shm_map(c->shm_handle);
    if ((uint64_t)c->ipc == (uint64_t)-1 || c->ipc == NULL) {
        sys_shm_destroy(c->shm_handle);
        c->shm_handle = 0;
        c->ipc = NULL;
        return;
    }
    
    c->ipc->width = WINDOW_MAX_WIDTH;
    c->ipc->height = WINDOW_MAX_HEIGHT;
    c->ipc->pitch_bytes = WINDOW_MAX_WIDTH * 4;
    c->ipc->key_head = 0;
    c->ipc->key_tail = 0;

    char handle_str[32];
    int i = 0;
    uint64_t temp = c->shm_handle;
    if (temp == 0) { handle_str[i++] = '0'; }
    else {
        while (temp > 0) { handle_str[i++] = (char)((temp % 10) + '0'); temp /= 10; }
    }
    handle_str[i] = '\0';

    for (int j = 0; j < i / 2; j++) { 
        char t = handle_str[j]; 
        handle_str[j] = handle_str[i - j - 1]; 
        handle_str[i - j - 1] = t; 
    }

    const char* args[] = {"/nvme/bin/idpterm.elf", handle_str, NULL};
    c->pid = sys_spawn(args[0], args);
    if (c->pid < 0) {
        sys_shm_unmap(c->ipc);
        sys_shm_destroy(c->shm_handle);
        c->ipc = NULL;
        c->shm_handle = 0;
        return;
    }

    wm->client_count++;
    wm->focused_index = idx;
    wm->dirty = 1;
}

static void wm_close_focused(wm_state_t* wm) {
    if (wm->client_count == 0) {
        return;
    }

    wm_client_t* victim = &wm->clients[wm->focused_index];
    if (victim->pid > 0) { 
        sys_kill(victim->pid); 
    }

    if (victim->ipc != NULL) {
        sys_shm_unmap(victim->ipc);
    }
    if (victim->shm_handle != 0) {
        sys_shm_destroy(victim->shm_handle);
    }

    for (uint8_t i = wm->focused_index; i + 1 < wm->client_count; ++i) {
        wm->clients[i] = wm->clients[i + 1];
    }

    wm->client_count--;
    if (wm->client_count == 0) {
        wm->focused_index = 0;
        return;
    }

    if (wm->focused_index >= wm->client_count) {
        wm->focused_index = (uint8_t)(wm->client_count - 1);
    }

    wm->dirty = 1;
}

static void wm_shutdown(wm_state_t* wm) {
    while (wm->client_count > 0) {
        wm->focused_index = 0;
        wm_close_focused(wm);
    }
}

static void wm_swap_focus_with_master(wm_state_t* wm) {
    if (wm->client_count < 2 || wm->focused_index == 0) {
        return;
    }

    wm_client_t tmp = wm->clients[0];
    wm->clients[0] = wm->clients[wm->focused_index];
    wm->clients[wm->focused_index] = tmp;
    wm->focused_index = 0;
}

static void wm_handle_key(wm_state_t* wm, const key_event_t* ev) {
    if (!ev->is_pressed) {
        return;
    }

    uint8_t alt = (ev->status_mask & ALT_MASK) != 0;
    uint8_t shift = (ev->status_mask & SHIFT_MASK) != 0;

    if (alt) {
        if (ev->code == KEY_ESC) {
            wm->running = 0;
            return;
        }
    
        if (shift && ev->code == KEY_ENTER) {
            wm->dirty = 1;
            wm_swap_focus_with_master(wm);
        } else if (ev->code == KEY_ENTER) {
            wm->dirty = 1;
            wm_spawn_terminal_client(wm);
        } else if (ev->code == KEY_Q) {
            wm->dirty = 1;
            wm_close_focused(wm);
        } else if (ev->code == KEY_SPACE) {
            wm->dirty = 1;
            wm->layout = (wm->layout == LAYOUT_MASTER_STACK) ? LAYOUT_MONOCLE : LAYOUT_MASTER_STACK;
        } else if (ev->code == KEY_TAB || ev->code == KEY_J || ev->code == KEY_L) {
            wm->dirty = 1;
            if (shift) wm_focus_prev(wm);
            else wm_focus_next(wm);
        } else if (ev->code == KEY_K || ev->code == KEY_H) {
            wm->dirty = 1;
            if (shift) wm_focus_next(wm);
            else wm_focus_prev(wm);
        } else if (ev->code == KEY_MINUS) {
            wm->dirty = 1;
            if (wm->master_ratio_percent > 35) wm->master_ratio_percent -= 5;
        } else if (ev->code == KEY_EQUAL) {
            wm->dirty = 1;
            if (wm->master_ratio_percent < 75) wm->master_ratio_percent += 5;
        } else if (ev->code >= KEY_1 && ev->code <= KEY_8) {
            wm->dirty = 1;
            uint8_t target = (uint8_t)(ev->code - KEY_1);
            if (target < wm->client_count) {
                wm->focused_index = target;
            }
        }
    } else {
        if (wm->client_count > 0) {
            wm_client_t* focused = &wm->clients[wm->focused_index];
            if (focused->ipc) {
                uint8_t next_head = (uint8_t)((focused->ipc->key_head + 1) % IPC_MAX_KEY_EVENTS);
                if (next_head != focused->ipc->key_tail) {
                    focused->ipc->key_ring[focused->ipc->key_head] = *ev;
                    focused->ipc->key_head = next_head;
                }
            }
        }
    }
}


void main() {
    gfx_context_t gfx = {0};
    wm_state_t wm = {0};

    if (gfx_init(&gfx) != ERR_SUCCESS) {
        sys_print("[IDPWM] libgfx initialization failed!\n");
        sys_exit(1);
    }

    if (gfx_load_font("/nvme/fonts/kryptonbold.psf", &g_title_font) != 0) {
        sys_print("[IDPWM] Failed to load window title font\n");
    }

    wm.gfx = &gfx;
    wm.layout = LAYOUT_MASTER_STACK;
    wm.master_ratio_percent = 50;
    wm.running = 1;

    wm_spawn_terminal_client(&wm);

    wm.dirty = 1;

    while (wm.running) {
        while (sys_keyboard_poll() > 0) {
            key_event_t ev;
            if (sys_keyboard_read(&ev) == ERR_SUCCESS) {
                wm_handle_key(&wm, &ev);
            }
        }

        int needs_render = wm.dirty;
        wm.dirty = 0;
        
        for (uint8_t i = 0; i < wm.client_count; ++i) {
            if (wm.clients[i].ipc && wm.clients[i].ipc->dirty) {
                needs_render = 1;
                wm.clients[i].ipc->dirty = 0;
            }
        }

        if (needs_render) {
            wm_arrange(&wm);
            wm_render(&wm);
        }
        
        sys_yield();
    }

    wm_shutdown(&wm);
    gfx_shutdown(&gfx);
    sys_exit(0);
}