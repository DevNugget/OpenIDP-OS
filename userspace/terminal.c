#include "openidp.h"

#define WM_PID 1 // Assuming WM is always the first user task
#define MSG_REQUEST_WINDOW 100 

// Messages TO Client
#define MSG_WINDOW_CREATED 200 // d1=w, d2=h, d3=buffer_ptr
#define MSG_WINDOW_RESIZE  201 // d1=w, d2=h

// Common
#define MSG_BUFFER_UPDATE  300

static uint32_t* fb_buffer = (void*)0;
static int fb_width = 0;
static int fb_height = 0;

void print_val(char* prefix, uint64_t val) {
    sys_write(0, prefix);
    char buf[32];
    int i = 0;
    if (val == 0) { buf[i++] = '0'; }
    else {
        char temp[32];
        int t = 0;
        while (val > 0) {
            temp[t++] = '0' + (val % 10);
            val /= 10;
        }
        while (t > 0) buf[i++] = temp[--t];
    }
    buf[i++] = '\n';
    buf[i] = 0;
    sys_write(0, buf);
}

// Simple graphics helper: Draw a gradient so we can see resizing happen
void draw_test_pattern() {
    if (!fb_buffer || fb_width <= 0 || fb_height <= 0) return;

    for (int y = 0; y < fb_height - 1; y++) {
        // Calculate pointer to the start of this row
        // Note: The WM expects the buffer to be tightly packed (stride = width)
        uint32_t* row = fb_buffer + (y * fb_width);

        for (int x = 0; x < fb_width - 1; x++) {
            // Generate a color based on position (X=Red, Y=Green)
            // Format: 00RRGGBB
            uint8_t r = (x * 255) / fb_width;
            uint8_t g = (y * 255) / fb_height;
            uint8_t b = 128; // Constant Blue

            row[x] = (r << 16) | (g << 8) | b;
        }
    }
}

void _start() {
    message_t msg;

    // 1. Handshake: politely ask the Window Manager for a surface.
    // We don't send size; the WM decides layout (tiling).
    sys_ipc_send(WM_PID, MSG_REQUEST_WINDOW, 0, 0, 0);

    for (;;) {
        // 2. Wait for Events
        // We block here to save CPU. The WM will wake us when something changes.
        int res = sys_ipc_recv(&msg);
        
        if (res == 0) {
            int needs_redraw = 0;

            if (msg.type == MSG_WINDOW_CREATED) {
                // WM allocated a buffer and assigned us a slot.
                fb_width = msg.data1;
                fb_height = msg.data2;
                fb_buffer = (uint32_t*)msg.data3; // This is our Virtual Address

                needs_redraw = 1;
            }
            else if (msg.type == MSG_WINDOW_RESIZE) {
                // WM changed our dimensions (e.g. another window opened/closed).
                // We reuse the existing buffer (it's full-screen sized), 
                // just update our render limits.
                fb_width = msg.data1;
                fb_height = msg.data2;
                
                needs_redraw = 1;
            }

            // 3. Render & Present
            if (needs_redraw && fb_buffer) {
                draw_test_pattern();
                
                // Tell WM we are done drawing.
                sys_ipc_send(WM_PID, MSG_BUFFER_UPDATE, 0, 0, 0);
            }
        }
    }
}