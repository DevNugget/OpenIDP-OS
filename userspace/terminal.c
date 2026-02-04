#include "openidp.h"
#include "libc/string.h"
#include "libc/heap.h"

// --- Constants & Structures ---

#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

#define COLOR_BG 0xFF101010
#define COLOR_FG 0xFFEEEEEE
#define FONT_PATH "/fonts/zap18.psf" 

typedef struct {
    uint8_t magic[2];
    uint8_t mode;
    uint8_t charsize;
} psf1_header_t;

typedef struct {
    psf1_header_t* header;
    void* glyph_buffer;
} psf1_font_t;

// --- State ---

static uint32_t* fb_buffer = NULL;
static int fb_width = 0;
static int fb_height = 0;

static psf1_font_t font;
static int char_w = 8;
static int char_h = 16;

static int cols = 0;
static int rows = 0;
static int cursor_x = 0;
static int cursor_y = 0;
static char* text_buffer = NULL; 

// --- Helpers ---

int load_font(void) {
    uint8_t* buffer = malloc(64 * 1024); 
    if (!buffer) return 0;

    int64_t size = sys_file_read(FONT_PATH, buffer, 64 * 1024);
    if (size <= 0) return 0;

    psf1_header_t* hdr = (psf1_header_t*)buffer;
    if (hdr->magic[0] != PSF1_MAGIC0 || hdr->magic[1] != PSF1_MAGIC1) return 0;

    font.header = hdr;
    font.glyph_buffer = (void*)(buffer + sizeof(psf1_header_t));
    char_h = hdr->charsize;
    char_w = 8;
    return 1;
}

// Low-level char drawer. 
// invert=1 swaps FG and BG (used for cursor)
void draw_char_cell(int cx, int cy, char c, int invert) {
    if (!fb_buffer || !font.glyph_buffer) return;
    
    // Bounds check
    if (cx < 0 || cx >= cols || cy < 0 || cy >= rows) return;

    uint32_t fg = invert ? COLOR_BG : COLOR_FG;
    uint32_t bg = invert ? COLOR_FG : COLOR_BG;

    uint8_t* glyph = (uint8_t*)font.glyph_buffer + (c * font.header->charsize);
    
    // Pre-calculate frame buffer offset to avoid mul in inner loop
    int start_px = cx * char_w;
    int start_py = cy * char_h;
    int pitch = fb_width; // In uint32 elements

    for (int y = 0; y < char_h; y++) {
        uint32_t* row_ptr = fb_buffer + ((start_py + y) * pitch) + start_px;
        uint8_t row_bits = glyph[y];
        
        // Unrolled bit loop for 8-pixel width (PSF1 standard)
        row_ptr[0] = (row_bits & 0x80) ? fg : bg;
        row_ptr[1] = (row_bits & 0x40) ? fg : bg;
        row_ptr[2] = (row_bits & 0x20) ? fg : bg;
        row_ptr[3] = (row_bits & 0x10) ? fg : bg;
        row_ptr[4] = (row_bits & 0x08) ? fg : bg;
        row_ptr[5] = (row_bits & 0x04) ? fg : bg;
        row_ptr[6] = (row_bits & 0x02) ? fg : bg;
        row_ptr[7] = (row_bits & 0x01) ? fg : bg;
    }
}

// Redraws the character UNDER the cursor (handling inversion logic)
void update_cursor_cell(void) {
    if (!text_buffer) return;
    char c = text_buffer[cursor_y * cols + cursor_x];
    if (c == 0) c = ' ';
    draw_char_cell(cursor_x, cursor_y, c, 1); // 1 = Invert
}

// Redraws a normal cell
void update_cell(int x, int y) {
    if (!text_buffer) return;
    char c = text_buffer[y * cols + x];
    if (c == 0) c = ' ';
    draw_char_cell(x, y, c, 0); // 0 = Normal
}

void term_scroll(void) {
    // 1. Pixel Scroll (Fastest way to move screen)
    // We copy rows 1..N to row 0..N-1
    int bytes_per_row = fb_width * 4;
    int scroll_height_pixels = (rows - 1) * char_h;
    
    uint32_t* dest = fb_buffer;
    uint32_t* src  = fb_buffer + (char_h * fb_width);
    
    // Move the bulk of the screen
    memcpy(dest, src, scroll_height_pixels * bytes_per_row);

    // 2. Clear the bottom pixel strip
    int bottom_opt_y = (rows - 1) * char_h;
    int clear_size = char_h * bytes_per_row;
    memset(fb_buffer + (bottom_opt_y * fb_width), 0x10, clear_size); // 0x10 approx dark grey/black

    // 3. Update Text Buffer Logic
    memcpy(text_buffer, text_buffer + cols, (rows - 1) * cols);
    memset(text_buffer + ((rows - 1) * cols), 0, cols);
    
    cursor_y--;
}

void term_notify_wm(int x, int y, int w, int h) {
    uint64_t d1 = ((uint64_t)x << 32) | (uint32_t)y;
    uint64_t d2 = ((uint64_t)w << 32) | (uint32_t)h;
    sys_ipc_send(1, MSG_BUFFER_UPDATE, d1, d2, 0);
}

void term_notify_cell(int cx, int cy) {
    term_notify_wm(cx * char_w, cy * char_h, char_w, char_h);
}

void term_write_char(char c) {
    int old_cx = cursor_x;
    int old_cy = cursor_y;

    // 1. Erase old cursor (Update buffer only, NO NOTIFY)
    update_cell(cursor_x, cursor_y); 
    
    // 2. Logic (Move cursor, update text buffer)
    if (c == '\n') {
        cursor_x = 0; cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) cursor_x--;
        else if (cursor_y > 0) { cursor_y--; cursor_x = cols - 1; }
        text_buffer[cursor_y * cols + cursor_x] = ' ';
        update_cell(cursor_x, cursor_y); // Update buffer only
    } else {
        text_buffer[cursor_y * cols + cursor_x] = c;
        update_cell(cursor_x, cursor_y); // Update buffer only
        cursor_x++;
        if (cursor_x >= cols) { cursor_x = 0; cursor_y++; }
    }

    // 3. Handle Scroll
    if (cursor_y >= rows) {
        term_scroll();
        term_notify_wm(0, 0, fb_width, fb_height); // Scroll needs full redraw
        return;
    }

    // 4. Draw new cursor (Update buffer only)
    update_cursor_cell();

    // 5. FINAL NOTIFY: Calculate the bounding box of the change
    // We affected (old_cx, old_cy) and (cursor_x, cursor_y)
    
    int min_x = (old_cx < cursor_x) ? old_cx : cursor_x;
    int min_y = (old_cy < cursor_y) ? old_cy : cursor_y;
    int max_x = (old_cx > cursor_x) ? old_cx : cursor_x;
    int max_y = (old_cy > cursor_y) ? old_cy : cursor_y;
    
    // Since it's usually just two adjacent cells, we can simply notify the 
    // union of the two character cells.
    int n_x = min_x * char_w;
    int n_y = min_y * char_h;
    int n_w = (max_x - min_x + 1) * char_w;
    int n_h = (max_y - min_y + 1) * char_h;

    // Send ONE message instead of THREE
    term_notify_wm(n_x, n_y, n_w, n_h);
}

// Re-renders EVERYTHING (Expensive! Use only on resize/init)
void term_redraw_all(void) {
    if (!text_buffer || !fb_buffer) return;
    
    // Clear background
    int size = fb_width * fb_height;
    for(int i=0; i<size; i++) fb_buffer[i] = COLOR_BG;

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            int invert = (x == cursor_x && y == cursor_y);
            char c = text_buffer[y * cols + x];
            if (c == 0) c = ' ';
            draw_char_cell(x, y, c, invert);
        }
    }
    sys_ipc_send(1, 300, 0, 0, 0);
}

void term_resize(int w, int h) {
    fb_width = w;
    fb_height = h;
    
    int new_cols = w / char_w;
    int new_rows = h / char_h;

    // Allocate new buffer
    char* new_text = malloc(new_cols * new_rows);
    memset(new_text, 0, new_cols * new_rows);

    // Copy old text if it exists (Optional improvement)
    if (text_buffer) {
        // Simple copy logic (top-left aligned)
        int copy_rows = (rows < new_rows) ? rows : new_rows;
        int copy_cols = (cols < new_cols) ? cols : new_cols;
        
        for(int y=0; y<copy_rows; y++) {
            memcpy(new_text + (y * new_cols), text_buffer + (y * cols), copy_cols);
        }
        free(text_buffer);
    } else {
        // First boot prompt
        char* p = "> ";
        int i=0;
        while(*p) new_text[i++] = *p++;
        cursor_x = 2; 
        cursor_y = 1;
    }

    text_buffer = new_text;
    cols = new_cols;
    rows = new_rows;

    term_redraw_all();
}

void handle_input(uint16_t key) {
    char c = (char)(key & 0xFF);
    if (c == 0) return;

    term_write_char(c);

    if (c == '\n') {
        char* p = "> ";
        while (*p) term_write_char(*p++);
    }
}

void _start() {
    if (!load_font()) sys_exit(1);

    sys_ipc_send(1, 100, 0, 0, 0); // Request Window

    message_t msg;
    for (;;) {
        if (sys_ipc_recv(&msg) == 0) {
            if (msg.type == MSG_QUIT_REQUEST) {
                // Free any client-side memory if you have it
                if (text_buffer) free(text_buffer);
                
                // The actual system call to kill this process
                sys_exit(0);
            }
            else if (msg.type == MSG_WINDOW_CREATED) { // Created
                fb_buffer = (uint32_t*)msg.data3;
                term_resize(msg.data1, msg.data2);
            }
            else if (msg.type == MSG_WINDOW_RESIZE) { // Resize
                term_resize(msg.data1, msg.data2);
            }
            else if (msg.type == MSG_KEY_EVENT) { // Key
                handle_input((uint16_t)msg.data1);
            }
        }
    }
}