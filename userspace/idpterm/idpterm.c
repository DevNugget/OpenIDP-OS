#include <libidp/syscall.h>
#include <libidp/window.h>
#include <libgfx/gfx.h>
#include <stdint.h>
#include <stddef.h>

#define MAX_COLS 256
#define MAX_ROWS 144
#define FONT_MAX_BYTES (256 * 1024)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t headersize;
    uint32_t flags;
    uint32_t numglyph;
    uint32_t bytesperglyph;
    uint32_t height;
    uint32_t width;
} psf2_header_t;

typedef struct { char ch; uint8_t fg, bg, dirty; } cell_t;

typedef struct {
    gfx_context_t* gfx;
    cell_t grid[MAX_ROWS][MAX_COLS];
    size_t rows;
    size_t cols;
    size_t row;
    size_t col;
    const uint8_t* glyphs;
    uint32_t bytes_per_glyph;
    uint32_t glyph_w;
    uint32_t glyph_h;
} term_t;

static uint8_t g_font_storage[FONT_MAX_BYTES];

static int psf2_parse(const uint8_t* blob, size_t len, const uint8_t** glyphs, uint32_t* bpg, uint32_t* w, uint32_t* h) {
    if (len < sizeof(psf2_header_t)) { sys_print("psf2 err: file too small or empty\n"); return -1; }
    
    const psf2_header_t* hdr = (const psf2_header_t*)blob;
    if (hdr->magic != 0x864ab572) { sys_print("psf2 err: bad magic number\n"); return -1; }
    if (hdr->headersize >= len) { sys_print("psf2 err: header size exceeds file len\n"); return -1; }
    if (hdr->bytesperglyph == 0 || hdr->numglyph == 0) { sys_print("psf2 err: 0 glyphs\n"); return -1; }
    
    if ((size_t)hdr->headersize + ((size_t)hdr->numglyph * (size_t)hdr->bytesperglyph) > len) { 
        sys_print("psf2 err: file is truncated\n"); 
        return -1; 
    }

    *glyphs = blob + hdr->headersize;
    *bpg = hdr->bytesperglyph;
    *w = hdr->width;
    *h = hdr->height;
    return 0;
}

static int load_font_from_fs(const char* path, const uint8_t** glyphs, uint32_t* bpg, uint32_t* w, uint32_t* h) {
    uint64_t fd = sys_open(path, IDP_O_RDONLY);
    if (fd == (uint64_t)ERR_FAIL) {
        sys_print("err: sys_open failed\n");
        return -1;
    }

    size_t offset = 0;
    while (offset < FONT_MAX_BYTES) {
        uint64_t rd = 0;
        if (sys_read(fd, g_font_storage + offset, FONT_MAX_BYTES - offset, &rd) != ERR_SUCCESS) {
            sys_close(fd);
            return -1;
        }
        if (rd == 0) break;
        offset += (size_t)rd;
    }

    sys_close(fd);
    return psf2_parse(g_font_storage, offset, glyphs, bpg, w, h);
}

static void term_clear(term_t* t) {
    for (size_t r = 0; r < t->rows; r++) {
        for (size_t c = 0; c < t->cols; c++) {
            t->grid[r][c] = (cell_t){' ', 15, 0, 1};
        }
    }
    t->row = 0;
    t->col = 0;
}

static void term_scroll(term_t* t) {
    for (size_t r = 1; r < t->rows; r++) {
        gfx_memcpy32((uint32_t*)&t->grid[r - 1][0], (const uint32_t*)&t->grid[r][0], t->cols);
    }

    for (size_t r = 0; r + 1 < t->rows; ++r) {
        for (size_t c = 0; c < t->cols; ++c) {
            t->grid[r][c].dirty = 1;
        }
    }

    for (size_t c = 0; c < t->cols; c++) {
        t->grid[t->rows-1][c] = (cell_t){' ', 15, 0, 1};
    }
    if (t->row) t->row--;
}

static void term_putc(term_t* t, char ch) {
    if (ch == '\n') { 
        t->col = 0; 
        t->row++; 
        if (t->row >= t->rows) term_scroll(t); 
        return; 
    }
    if (ch == '\r') { t->col = 0; return; }
    if (ch == '\b') { 
        if (t->col) t->col--; 
        t->grid[t->row][t->col] = (cell_t){' ', 15, 0, 1};
        return; 
    }
    
    t->grid[t->row][t->col] = (cell_t){ch, 15, 0, 1};
    if (++t->col >= t->cols) { 
        t->col = 0; 
        t->row++; 
        if (t->row >= t->rows) term_scroll(t); 
    }
}

static uint32_t color(uint8_t idx){ return idx?gfx_rgb(220,220,220):gfx_rgb(20,24,32); }

static void draw_cell(term_t* t, size_t r, size_t c) {
    cell_t cell = t->grid[r][c];
    int x = (int)(c * t->glyph_w);
    int y = (int)(r * t->glyph_h);
    
    uint32_t bg_color = color(cell.bg);
    uint32_t fg_color = color(cell.fg);

    gfx_fill_rect(t->gfx, x, y, t->glyph_w, t->glyph_h, bg_color);
    
    const uint8_t* g = t->glyphs + ((uint8_t)cell.ch * t->bytes_per_glyph);
    
    int bytes_per_row = (t->glyph_w + 7) / 8;

    for (uint32_t gy = 0; gy < t->glyph_h; gy++) {
        for (uint32_t gx = 0; gx < t->glyph_w; gx++) {
            uint8_t byte = g[gy * (uint32_t)bytes_per_row + (gx / 8)];
            if (byte & (0x80 >> (gx % 8))) {
                t->gfx->back_buffer[(y + (int)gy) * t->gfx->stride_pixels + (x + (int)gx)] = fg_color;
            }
        }
    }
}

static void term_render(term_t* t) {
    for(size_t r = 0; r < t->rows; r++) {
        for(size_t c = 0; c < t->cols; c++) {
            if (t->grid[r][c].dirty) {
                draw_cell(t, r, c);
                t->grid[r][c].dirty = 0;
            }
        }
    }
}

static void u64_to_hex(uint64_t val, char* buf) {
    const char* hex = "0123456789ABCDEF";
    int i = 15;
    buf[16] = '\0';
    do {
        buf[i--] = hex[val % 16];
        val /= 16;
    } while (i >= 0);
}

static char get_ascii_char(uint8_t code, uint8_t shift) {
    if (code >= KEY_A && code <= KEY_Z) {
        return shift ? (char)('A' + (code - KEY_A)) : (char)('a' + (code - KEY_A));
    }
    if (code >= KEY_1 && code <= KEY_9) {
        const char* unshifted = "123456789";
        const char* shifted   = "!@#$%^&*(";
        return shift ? shifted[code - KEY_1] : unshifted[code - KEY_1];
    }
    if (code == KEY_0) return shift ? ')' : '0';
    
    switch (code) {
        case KEY_MINUS:     return shift ? '_' : '-';
        case KEY_EQUAL:     return shift ? '+' : '=';
        case KEY_LBRACKET:  return shift ? '{' : '[';
        case KEY_RBRACKET:  return shift ? '}' : ']';
        case KEY_SEMICOLON: return shift ? ':' : ';';
        case KEY_QUOTE:     return shift ? '"' : '\'';
        case KEY_BACKTICK:  return shift ? '~' : '`';
        case KEY_BACKSLASH: return shift ? '|' : '\\';
        case KEY_COMMA:     return shift ? '<' : ',';
        case KEY_DOT:       return shift ? '>' : '.';
        case KEY_SLASH:     return shift ? '?' : '/';
        case KEY_SPACE:     return ' ';
        case KEY_ENTER:     return '\n';
        case KEY_BACKSPACE: return '\b';
        case KEY_TAB:       return '\t';
        default:            return 0;
    }
}

void main(int argc, char** argv) {
    if (argc < 2 || argv == NULL || argv[1] == NULL) {
        sys_print("Terminal error: Missing SHM handle argument\n");
        sys_exit(1);
    }

    uint64_t handle = 0;
    for (int i = 0; argv[1][i] != '\0'; i++) {
        handle = handle * 10 + (uint64_t)(argv[1][i] - '0');
    }

    window_ipc_t* ipc = (window_ipc_t*)sys_shm_map(handle);
    if ((uint64_t)ipc == (uint64_t)-1) sys_exit(1);

    gfx_context_t gfx;
    for (size_t i = 0; i < sizeof(gfx); ++i) ((uint8_t*)&gfx)[i] = 0;
    
    uint64_t bb_handle = sys_shm_create(WINDOW_MAX_WIDTH * WINDOW_MAX_HEIGHT * 4);
    if ((int64_t)bb_handle < 0) {
        sys_shm_unmap(ipc);
        sys_exit(1);
    }
    gfx.back_buffer = (uint32_t*)sys_shm_map(bb_handle);
    if ((uint64_t)gfx.back_buffer == (uint64_t)-1 || gfx.back_buffer == NULL) {
        sys_shm_destroy(bb_handle);
        sys_shm_unmap(ipc);
        sys_exit(1);
    }

    gfx.front_buffer = ipc->pixels;
    gfx.width = WINDOW_MAX_WIDTH;
    gfx.height = WINDOW_MAX_HEIGHT;
    gfx.pitch_bytes = WINDOW_MAX_WIDTH * 4;
    gfx.stride_pixels = WINDOW_MAX_WIDTH;
    gfx.bpp = 32;

    term_t term;
    for (size_t i = 0; i < sizeof(term); ++i) ((uint8_t*)&term)[i] = 0;
    term.gfx = &gfx;

    if (load_font_from_fs("/nvme/fonts/krypton.psf", &term.glyphs, &term.bytes_per_glyph, &term.glyph_w, &term.glyph_h) != 0) {
        sys_print("idpterm: failed to load font\n");
        sys_exit(2);
    }

    term.cols = ipc->width / term.glyph_w;
    term.rows = ipc->height / term.glyph_h;
    if (term.cols > MAX_COLS) term.cols = MAX_COLS;
    if (term.rows > MAX_ROWS) term.rows = MAX_ROWS;
    if (term.cols == 0) term.cols = 1;
    if (term.rows == 0) term.rows = 1;

    term_clear(&term);
    const char* banner = "idpterm ready\n";
    for (size_t i=0; banner[i]; ++i) term_putc(&term, banner[i]);

    uint64_t shell_in_r, shell_in_w;
    uint64_t shell_out_r, shell_out_w;
    if (sys_pipe(&shell_in_r, &shell_in_w) != ERR_SUCCESS || sys_pipe(&shell_out_r, &shell_out_w) != ERR_SUCCESS) {
        sys_shm_unmap(gfx.back_buffer);
        sys_shm_destroy(bb_handle);
        sys_shm_unmap(ipc);
        sys_exit(1);
    }

    char arg_in[20], arg_out[20];
    u64_to_hex(shell_in_r, arg_in);
    u64_to_hex(shell_out_w, arg_out);

    const char* shell_args[] = {"/nvme/bin/idpshell.elf", arg_in, arg_out, NULL};
    int shell_pid = sys_spawn(shell_args[0], shell_args);
    if (shell_pid < 0) {
        sys_close(shell_in_r);
        sys_close(shell_in_w);
        sys_close(shell_out_r);
        sys_close(shell_out_w);
        sys_shm_unmap(gfx.back_buffer);
        sys_shm_destroy(bb_handle);
        sys_shm_unmap(ipc);
        sys_exit(1);
    }

    while (1) {
        int needs_render = 0;

        uint32_t new_cols = ipc->width / term.glyph_w;
        uint32_t new_rows = ipc->height / term.glyph_h;
        
        if (new_cols > MAX_COLS) new_cols = MAX_COLS;
        if (new_rows > MAX_ROWS) new_rows = MAX_ROWS;
        if (new_cols == 0) new_cols = 1;
        if (new_rows == 0) new_rows = 1;

        if (term.cols != new_cols || term.rows != new_rows) {
            term.cols = new_cols;
            term.rows = new_rows;
            
            if (term.col >= term.cols) term.col = term.cols - 1;
            if (term.row >= term.rows) term.row = term.rows - 1;
            
            gfx_fill_rect(term.gfx, 0, 0, ipc->width, ipc->height, color(0));
            
            for (size_t r = 0; r < term.rows; r++) {
                for (size_t c = 0; c < term.cols; c++) {
                    term.grid[r][c].dirty = 1;
                }
            }
            needs_render = 1;
        }

        if (ipc->key_tail != ipc->key_head) {
            key_event_t ev = ipc->key_ring[ipc->key_tail];
            ipc->key_tail = (uint8_t)((ipc->key_tail + 1) % IPC_MAX_KEY_EVENTS);

            if (ev.is_pressed) {
                uint8_t shift = (ev.status_mask & SHIFT_MASK) != 0;
                char ch = get_ascii_char(ev.code, shift);
                
                if (ch != 0) {
                    uint64_t wr;
                    sys_write(shell_in_w, &ch, 1, &wr);
                }
            }
        }

        char buf[64];
        uint64_t rd = 0;
        sys_read(shell_out_r, buf, sizeof(buf), &rd);
        
        if (rd > 0) {
            for (uint64_t i = 0; i < rd; i++) {
                term_putc(&term, buf[i]);
            }
            needs_render = 1;
        }
        
        if (needs_render) {
            term_render(&term); 
            gfx_present(&gfx);
            ipc->dirty = 1;
        }
        
        sys_yield();
    }
}
