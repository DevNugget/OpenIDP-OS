#include "stdio.h"
#include "../openidp.h"
#include "../libc/string.h"
#include <stdarg.h>

#define BATCH_SIZE      24

int terminal_pid = -1;

static char out_buf[BATCH_SIZE + 1]; // +1 for null terminator safety
static int buf_idx = 0;

void stdio_init(void) {
    message_t msg;
    // BLOCK until we receive the handshake from our parent Terminal
    while (1) {
        if (sys_ipc_recv(&msg) == 0) {
            if (msg.type == MSG_HANDSHAKE && msg.data1 == 0) {
                terminal_pid = msg.sender_pid;
                return; // Connection established!
            } else if (msg.type == MSG_HANDSHAKE && msg.data1 != 0) {
                terminal_pid = msg.data1;
                return;
            }
        }
    }
}

static void fflush_internal(void) {
    if (buf_idx == 0) return;
    if (terminal_pid == -1) return;

    // Zero out the rest of the buffer so we don't send garbage
    for (int i = buf_idx; i < BATCH_SIZE; i++) {
        out_buf[i] = 0;
    }

    // PACKING: Treat the buffer as 3 uint64_t integers
    // We cast the char pointer to a uint64_t pointer to read 8 bytes at a time
    uint64_t* packs = (uint64_t*)out_buf;
    uint64_t d1 = packs[0];
    uint64_t d2 = packs[1];
    uint64_t d3 = packs[2];

    // RETRY LOOP (Backpressure)
    int ret;
    do {
        ret = sys_ipc_send(terminal_pid, MSG_STDOUT_BATCH, d1, d2, d3);
        if (ret != 0) asm volatile("pause");
    } while (ret != 0);

    // Reset buffer
    buf_idx = 0;
    // Clear buffer again to be safe (optional but good for debugging)
    memset(out_buf, 0, BATCH_SIZE);
}

void putchar(char c) {
    // Add to buffer
    out_buf[buf_idx++] = c;

    // Flush if:
    // 1. Buffer is FULL (24 chars)
    // 2. Character is NEWLINE (Line buffering is standard for terminals)
    if (buf_idx >= BATCH_SIZE || c == '\n') {
        fflush_internal();
    }
}

void fflush(void) {
    fflush_internal();
}

char getchar(void) {
    fflush_internal();
    message_t msg;
    while (1) {
        int res = sys_ipc_recv(&msg);
        // Only accept keys from our connected terminal
        if (res == 0 && msg.type == MSG_KEY_EVENT && msg.sender_pid == terminal_pid) {
            return (char)msg.data1;
        }

        if (msg.type == MSG_QUIT_REQUEST) {
            return EOF; 
        }
    }
}

void printf(const char* fmt, ...) {
    // (Implementation same as previous example, calls putchar)
    va_list args;
    va_start(args, fmt);
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 's') {
                const char* s = va_arg(args, const char*);
                while (*s) putchar(*s++);
            } 
             // Add other formatters...
        } else {
            putchar(*fmt);
        }
        fmt++;
    }
    va_end(args);
}

void clear_screen() {
    sys_ipc_send(terminal_pid, MSG_STDOUT_CLEAR, 0, 0, 0);
}