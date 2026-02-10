#include "../openidp.h"
#include "../libc/heap.h"
#include "../libc/string.h"
#include <stddef.h>         // Fixed: Needed for size_t
#include "../libgfx/gfx.h"  
#include "term.h"

// Window manager PID is always 1 (Prone to change in the future)
#define WM_PID 1 

#define FONT_PATH "/fonts/krypton.psf"

static gfx_context_t gfx;
static term_t term;
static int shell_pid = -1;

/* Helper functions */

// Robust file loader (allocates buffer)
static void* load_file_to_memory(const char* path) {
    struct kstat stat;

    int stat_err = sys_stat(path, &stat);
    if (stat_err != 0 || stat.flags == 1 || stat.size == 0) {
        return NULL;
    }

    uint8_t* buffer = malloc(stat.size); 
    
    if (!buffer) return NULL;

    int64_t bytes_read = sys_file_read(path, buffer, stat.size);
    
    if (bytes_read != stat.size) { 
        free(buffer); 
        return NULL; 
    }
    
    return buffer;
}

// Handles the dirty rectangle returned by term operations
// This prevents IPC flooding by sending only one update per event
static void process_term_update(term_t* term) {
    if (term->last_update.dirty) {
        rect_t* update = &term->last_update;

        uint64_t pos_data = ((uint64_t)update->x << 32) | (uint32_t)update->y;
        uint64_t size_data = ((uint64_t)update->w << 32) | (uint32_t)update->h;

        sys_ipc_send(1, MSG_BUFFER_UPDATE, pos_data, size_data, 0);

        term->last_update.dirty = 0;
    }
}

void _start() {
    void* font_data = load_file_to_memory(FONT_PATH);
    if (!font_data) sys_exit(1);

    // Handshake with window manager
    sys_ipc_send(1, MSG_REQUEST_WINDOW, 0, 0, 0); 

    message_t msg;
    int running = 1;
    int initialized = 0;

    while (running) {
        if (sys_ipc_recv(&msg) == 0) {
            
            // Handle regardless of state
            if (msg.type == MSG_QUIT_REQUEST) {
                if (shell_pid > 0) {
                    // Forward the kill signal to the shell
                    sys_ipc_send(shell_pid, MSG_QUIT_REQUEST, 0, 0, 0);
                }

                running = 0;
                continue;
            }

            // Handle creation if not initialized
            if (msg.type == MSG_WINDOW_CREATED && !initialized) {
                uint32_t* fb = (uint32_t*)msg.data3;
                int width = (int)msg.data1;
                int height = (int)msg.data2;

                if (gfx_init(&gfx, font_data, fb, width, height) != 0) {
                    sys_exit(2);
                }

                term_init(&term, &gfx);
                char* shell_argv[] = {
                    "/",                   // argv[0]: CWD (Root)
                    "/bin/idpshell.elf",   // argv[1]: Program Name
                    NULL                   // Null terminator
                };
                int shell_argc = 2;
                shell_pid = sys_exec("/bin/idpshell.elf", shell_argc, shell_argv);
                sys_ipc_send(shell_pid, MSG_HANDSHAKE, 0, 0, 0);

                initialized = 1;

                term_write(&term, GFX_VER);
                term_write(&term, TERM_VER);

                process_term_update(&term);
                process_term_update(&term);
                continue;
            }

            // Ignore IPC messages if terminal not intialized
            if (!initialized) continue;

            switch (msg.type) {
                case MSG_WINDOW_RESIZE: {
                    int w = (int)msg.data1;
                    int h = (int)msg.data2;

                    // Update Contexts
                    gfx.width = w;
                    gfx.height = h;
                    gfx.pitch = w;

                    term_resize(&term, w, h);
                    
                    // Resize invalidates the whole screen, notify WM
                    // For resize, force manual update
                    rect_t full = {0, 0, w, h, 1};
                    // Manually send this one since it's not in term->last_update
                    sys_ipc_send(1, MSG_BUFFER_UPDATE, 0, ((uint64_t)w << 32) | h, 0);
                    break;
                }

                case MSG_KEY_EVENT: {
		            if (shell_pid > 0) {
                        sys_ipc_send(shell_pid, MSG_KEY_EVENT, msg.data1, 0, 0);
                    }
                    break;
                }

                case MSG_STDOUT: {
                    char c = (char)msg.data1;
                    char str[2] = { c, 0 };
                    
                    // Draw to the terminal buffer
                    term_write(&term, str);
                    
                    // Notify WM
                    process_term_update(&term);
                    break;    		
                }

                case MSG_STDOUT_BATCH: {
                    char batch_str[25]; 
                    memset(batch_str, 0, 25);

                    // Unpack the registers back into memory
                    uint64_t* packer = (uint64_t*)batch_str;
                    packer[0] = msg.data1;
                    packer[1] = msg.data2;
                    packer[2] = msg.data3;

                    // Draw the whole string at once
                    term_write(&term, batch_str);
                    
                    // Notify WM
                    process_term_update(&term);
                    break;
                }

                case MSG_STDOUT_CLEAR: {
                    term_clear(&term);
                    break;
                }
            
                default:
                    // Ignore unknown messages
                    break;
            }
        }
    }

    // Cleanup
    term_destroy(&term);
    free(font_data);
    sys_exit(0);
}
