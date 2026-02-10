#ifndef IDP_TERM_H
#define IDP_TERM_H

#include "../libc/stdint.h"
#include "../libgfx/gfx.h"

#define TERM_VER  "--> idpterm "

// Defines a rectangular dirty area that needs repainting
typedef struct {
    int x, y, w, h;
    int dirty; // 1 if this rect needs to be sent to WM
} rect_t;

typedef enum {
    ANSI_STATE_NORMAL,
    ANSI_STATE_ESC,
    ANSI_STATE_CSI, // Control Sequence Introducer '['
    ANSI_STATE_OSC
} ansi_state_t;

// A single cell on screen
typedef struct {
    char c;
    uint32_t fg;
    uint32_t bg;
} term_cell_t;

typedef struct {
    gfx_context_t* gfx;

    // Grid State
    term_cell_t* text_buffer; // Character grid (rows * cols)
    int cols;                   
    int rows;               
    
    // Cursor State
    int cursor_x;
    int cursor_y;
    int show_cursor;

    // Visuals
    uint32_t fg_color;
    uint32_t bg_color;

    // Update Tracking
    rect_t last_update; // Stores the region modified by the last operation

    ansi_state_t ansi_state;
    int ansi_params[4];
    int ansi_param_count;

    char osc_buffer[128]; // Buffer for file path
    int osc_idx;
} term_t;

void term_init(term_t* term, gfx_context_t* gfx);
void term_resize(term_t* term, int width, int height);
void term_destroy(term_t* term);
void term_clear(term_t* term);
rect_t term_write(term_t* term, const char* str);
void term_redraw_full(term_t* term);
void term_set_cursor_visible(term_t* term, int visible);

#endif