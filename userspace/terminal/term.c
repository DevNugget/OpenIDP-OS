#include "term.h"
#include "../libc/string.h"
#include "../libc/heap.h"

#define DEFAULT_BG 0x00181825
#define DEFAULT_FG 0x00bac2de

// ANSI Color Palette (Standard 0-7)
static const uint32_t ANSI_COLORS[] = {
    0x00181825, // 0: Black
    0x00f38ba8, // 1: Red
    0x00a6e3a1, // 2: Green
    0x00f9e2af, // 3: Yellow
    0x0089b4fa, // 4: Blue
    0x00cba6f7, // 5: Magenta
    0x006c7086, // 6: Cyan
    0x00bac2de  // 7: White
};

/* --- Helper Functions --- */

static void add_dirty_cell(term_t* term, int col, int row) {
    int cell_w = gfx_font_width(term->gfx);
    int cell_h = gfx_font_height(term->gfx);
    
    int x = col * cell_w;
    int y = row * cell_h;

    rect_t* r = &term->last_update;

    if (!r->dirty) {
        r->x = x; r->y = y;
        r->w = cell_w; r->h = cell_h;
        r->dirty = 1;
    } else {
        int min_x = (x < r->x) ? x : r->x;
        int min_y = (y < r->y) ? y : r->y;
        int max_x = ((x + cell_w) > (r->x + r->w)) ? (x + cell_w) : (r->x + r->w);
        int max_y = ((y + cell_h) > (r->y + r->h)) ? (y + cell_h) : (r->y + r->h);

        r->x = min_x;
        r->y = min_y;
        r->w = max_x - min_x;
        r->h = max_y - min_y;
    }
}

// Draws a single cell based on text buffer content
static void draw_cell(term_t* term, int x, int y) {
    if (!term->text_buffer) return;
    
    // Retrieve the specific cell info
    term_cell_t* cell = &term->text_buffer[y * term->cols + x];
    char c = cell->c;
    if (c == 0) c = ' ';

    // NOTE: Cursor inverts the specific cell colors
    int is_cursor = (term->show_cursor && x == term->cursor_x && y == term->cursor_y);
    
    uint32_t fg = is_cursor ? cell->bg : cell->fg;
    uint32_t bg = is_cursor ? cell->fg : cell->bg;

    int px = x * gfx_font_width(term->gfx);
    int py = y * gfx_font_height(term->gfx);

    gfx_draw_char(term->gfx, px, py, c, fg, bg); 
    
    add_dirty_cell(term, x, y);
}

static void term_scroll(term_t* term) {
    if (!term->text_buffer) return;

    int row_size_bytes = term->cols * sizeof(term_cell_t);
    int num_rows_to_move = term->rows - 1;
    
    // Move Text Buffer
    memcpy(term->text_buffer, 
           term->text_buffer + term->cols, 
           num_rows_to_move * row_size_bytes);

    // Clear last row
    term_cell_t* last_row = term->text_buffer + (num_rows_to_move * term->cols);
    for (int i = 0; i < term->cols; i++) {
        last_row[i].c = 0;
        last_row[i].fg = term->fg_color; // Fill with current background color
        last_row[i].bg = term->bg_color; // to avoid "stripes" of old color
    }

    if (term->cursor_y > 0) term->cursor_y--;

    term_redraw_full(term);
}

/* --- ANSI Parser --- */

static void apply_ansi_code(term_t* term, int code) {
    if (code == 0) {
        // Reset
        term->fg_color = DEFAULT_FG;
        term->bg_color = DEFAULT_BG;
    } 
    else if (code >= 30 && code <= 37) {
        // Foreground
        term->fg_color = ANSI_COLORS[code - 30];
    }
    else if (code >= 40 && code <= 47) {
        // Background
        term->bg_color = ANSI_COLORS[code - 40];
    }
}

static void term_process_ansi_end(term_t* term, char terminator) {
    // We only support 'm' (SGR - Select Graphic Rendition) for now
    if (terminator == 'm') {
        if (term->ansi_param_count == 0) {
            // treat \033[m as \033[0m
            apply_ansi_code(term, 0);
        } else {
            for (int i = 0; i < term->ansi_param_count; i++) {
                apply_ansi_code(term, term->ansi_params[i]);
            }
        }
    }
    
    // Reset state
    term->ansi_state = ANSI_STATE_NORMAL;
    term->ansi_param_count = 0;
    term->ansi_params[0] = 0;
}

/* --- Public API --- */

void term_init(term_t* term, gfx_context_t* gfx) {
    term->gfx = gfx;
    term->fg_color = DEFAULT_FG;
    term->bg_color = DEFAULT_BG;
    term->cursor_x = 0;
    term->cursor_y = 0;
    term->show_cursor = 1;
    term->text_buffer = NULL;
    
    term->ansi_state = ANSI_STATE_NORMAL;
    term->ansi_param_count = 0;
    
    term_resize(term, gfx->width, gfx->height);
}

void term_destroy(term_t* term) {
    if (term->text_buffer) {
        free(term->text_buffer);
        term->text_buffer = NULL;
    }
}

void term_resize(term_t* term, int width, int height) {
    int char_w = gfx_font_width(term->gfx);
    int char_h = gfx_font_height(term->gfx);
    int new_cols = width / char_w;
    int new_rows = height / char_h;

    term_cell_t* new_buf = malloc(new_cols * new_rows * sizeof(term_cell_t));
    
    // Initialize new buffer
    for(int i = 0; i < new_cols * new_rows; i++) {
        new_buf[i].c = 0;
        new_buf[i].fg = DEFAULT_FG;
        new_buf[i].bg = DEFAULT_BG;
    }

    if (term->text_buffer) {
        int copy_rows = (term->rows < new_rows) ? term->rows : new_rows;
        int copy_cols = (term->cols < new_cols) ? term->cols : new_cols;

        for (int y = 0; y < copy_rows; y++) {
            // Copy row data
            memcpy(new_buf + (y * new_cols), 
                   term->text_buffer + (y * term->cols), 
                   copy_cols * sizeof(term_cell_t));
        }
        free(term->text_buffer);
    }

    term->text_buffer = new_buf;
    term->cols = new_cols;
    term->rows = new_rows;

    if (term->cursor_x >= new_cols) term->cursor_x = new_cols - 1;
    if (term->cursor_y >= new_rows) term->cursor_y = new_rows - 1;

    term_redraw_full(term);
}

void term_clear(term_t* term) {
    if (term->text_buffer) {
        for(int i = 0; i < term->cols * term->rows; i++) {
            term->text_buffer[i].c = 0;
            term->text_buffer[i].fg = term->fg_color;
            term->text_buffer[i].bg = term->bg_color;
        }
    }
    term->cursor_x = 0;
    term->cursor_y = 0;
    term_redraw_full(term);
}

void term_redraw_full(term_t* term) {
    // Clear with default background first
    gfx_clear(term->gfx, DEFAULT_BG);
    
    for (int y = 0; y < term->rows; y++) {
        for (int x = 0; x < term->cols; x++) {
            term_cell_t* cell = &term->text_buffer[y * term->cols + x];
            
            // Draw if character exists OR if bg color is not default 
            if ((cell->c != 0 && cell->c != ' ') || cell->bg != DEFAULT_BG) {
                draw_cell(term, x, y);
            }
            
            if (term->show_cursor && x == term->cursor_x && y == term->cursor_y) {
                draw_cell(term, x, y);
            }
        }
    }

    term->last_update.x = 0;
    term->last_update.y = 0;
    term->last_update.w = term->gfx->width;
    term->last_update.h = term->gfx->height;
    term->last_update.dirty = 1;
}

void term_set_cursor_visible(term_t* term, int visible) {
    if (term->show_cursor != visible) {
        draw_cell(term, term->cursor_x, term->cursor_y); 
        term->show_cursor = visible;
        draw_cell(term, term->cursor_x, term->cursor_y);
    }
}

// Helper to write a single visible char
static void term_put_char(term_t* term, char c) {
    int old_x = term->cursor_x;
    int old_y = term->cursor_y;

    if (c == '\n') {
        term->cursor_x = 0;
        term->cursor_y++;
    } 
    else if (c == '\b') {
        if (term->cursor_x > 0) {
            term->cursor_x--;
        } else if (term->cursor_y > 0) {
            term->cursor_y--;
            term->cursor_x = term->cols - 1;
        }
        // Erase character and attributes
        term_cell_t* cell = &term->text_buffer[term->cursor_y * term->cols + term->cursor_x];
        cell->c = 0;
        cell->fg = term->fg_color;
        cell->bg = term->bg_color;
    } 
    else {
        // Write char with CURRENT attributes
        term_cell_t* cell = &term->text_buffer[old_y * term->cols + old_x];
        cell->c = c;
        cell->fg = term->fg_color;
        cell->bg = term->bg_color;
        
        term->cursor_x++;
        
        if (term->cursor_x >= term->cols) {
            term->cursor_x = 0;
            term->cursor_y++;
        }
    }

    if (term->cursor_y >= term->rows) {
        term_scroll(term); 
        return;
    }

    draw_cell(term, old_x, old_y);
    draw_cell(term, term->cursor_x, term->cursor_y);
}

rect_t term_write(term_t* term, const char* str) {
    while (*str) {
        char c = *str++;
        
        // ESCAPE state
        if (term->ansi_state == ANSI_STATE_ESC) {
            if (c == '[') {
                term->ansi_state = ANSI_STATE_CSI;
                term->ansi_param_count = 0;
                term->ansi_params[0] = 0;
            } else {
                // Invalid escape, fallback to print both
                // (Very simplified, ideally you handle other ESC sequences)
                term->ansi_state = ANSI_STATE_NORMAL;
                term_put_char(term, c);
            }
            continue;
        }
        
        // CSI state
        else if (term->ansi_state == ANSI_STATE_CSI) {
            if (c >= '0' && c <= '9') {
                term->ansi_params[term->ansi_param_count] = 
                    (term->ansi_params[term->ansi_param_count] * 10) + (c - '0');
            } 
            else if (c == ';') {
                if (term->ansi_param_count < 3) { // Limit max params to avoid overflow
                    term->ansi_param_count++;
                    term->ansi_params[term->ansi_param_count] = 0;
                }
            } 
            else {
                // Final byte (e.g., 'm' for SGR)
                if (term->ansi_param_count == 0 && term->ansi_params[0] == 0) {
                   // Handle empty case [m
                } else {
                   term->ansi_param_count++; // Count the last parameter
                }
                term_process_ansi_end(term, c);
            }
            continue;
        }
        
        // NORMAL state
        if (c == '\x1b') {
            term->ansi_state = ANSI_STATE_ESC;
        } else {
            term_put_char(term, c);
        }
    }

    return term->last_update;
}