/*
 * TODO: Refactor this mess of a file
 * 
 */

#include "idpwm.h"
#include "openidp.h"
#include "libc/heap.h"
#include "libc/string.h"

typedef struct {
    int min_x, min_y;
    int max_x, max_y;
    int is_dirty;
} dirty_state_t;

static dirty_state_t dirty_map[MAX_WINDOWS];

// Helper to reset a dirty slot
void reset_dirty(int i) {
    dirty_map[i].min_x = 100000; // Arbitrary large number
    dirty_map[i].min_y = 100000;
    dirty_map[i].max_x = -1;
    dirty_map[i].max_y = -1;
    dirty_map[i].is_dirty = 0;
}

// Helper to mark area as dirty
void mark_dirty(int slot, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (x < dirty_map[slot].min_x) dirty_map[slot].min_x = x;
    if (y < dirty_map[slot].min_y) dirty_map[slot].min_y = y;
    // Calculate max (exclusive)
    if (x + w > dirty_map[slot].max_x) dirty_map[slot].max_x = x + w;
    if (y + h > dirty_map[slot].max_y) dirty_map[slot].max_y = y + h;
    dirty_map[slot].is_dirty = 1;
}

// --- Definitions & Helpers ---

#ifndef NULL
#define NULL ((void*)0)
#endif

// Tree splitting directions
typedef enum { SPLIT_H, SPLIT_V } split_dir_t;

// The Node Structure
typedef struct tree_node {
    struct tree_node *parent;
    struct tree_node *left;
    struct tree_node *right;
    int is_leaf;
    split_dir_t split_dir;
    window_t *win;
    int x, y, w, h;
} tree_node_t;

// --- Globals ---

static window_t windows[MAX_WINDOWS];
static tree_node_t nodes[MAX_WINDOWS * 2];
static int node_pool_idx = 0;

// FIX: Free list to recycle nodes and prevent memory exhaustion
static int free_stack[MAX_WINDOWS * 2];
static int free_stack_top = -1;

static tree_node_t *root = NULL;
static tree_node_t *focused_node = NULL;
static split_dir_t next_split_mode = SPLIT_V;

static uint32_t *back_buffer;
static int back_buffer_size;
static int screen_pitch_bytes; // Pre-calculated

// --- Prototypes ---
void wm_draw(void);
void wm_present(void);

// ----------------------------------------------------------------------------
//  Graphics Helpers (Optimized)
// ----------------------------------------------------------------------------

void wm_present_rect(int x, int y, int w, int h) {
    // 1. Clip
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > framebuffer.fb_width) w = framebuffer.fb_width - x;
    if (y + h > framebuffer.fb_height) h = framebuffer.fb_height - y;
    if (w <= 0 || h <= 0) return;

    // 2. Fast Blit
    // Convert pointers to bytes to handle pitch correctly
    uint8_t *dst_base = (uint8_t*)framebuffer.fb_addr + (y * framebuffer.fb_pitch) + (x * 4);
    uint8_t *src_base = (uint8_t*)back_buffer + (y * screen_pitch_bytes) + (x * 4);
    
    int row_bytes = w * 4;

    for (int i = 0; i < h; i++) {
        memcpy(dst_base, src_base, row_bytes);
        dst_base += framebuffer.fb_pitch;
        src_base += screen_pitch_bytes;
    }
}

void fb_fill_rect(uint32_t* fb_ptr, int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    
    // Bounds Check
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > framebuffer.fb_width) w = framebuffer.fb_width - x;
    if (y + h > framebuffer.fb_height) h = framebuffer.fb_height - y;

    // Pre-calculate stride (pixels per row)
    // Note: screen_pitch_bytes is usually (width * 4), but can be larger for alignment
    int stride_pixels = screen_pitch_bytes / 4; 

    for (int yy = 0; yy < h; yy++) {
        uint32_t *row = fb_ptr + ((y + yy) * stride_pixels) + x;
        
        // Use a loop to set 32-bit values correctly
        for (int xx = 0; xx < w; xx++) {
            row[xx] = color;
        }
    }
}

void fb_draw_border(uint32_t* fb, int x, int y, int w, int h, uint32_t color, int thick) {
    if (w < thick*2 || h < thick*2) return;
    // Top & Bottom
    fb_fill_rect(fb, x, y, w, thick, color);
    fb_fill_rect(fb, x, y + h - thick, w, thick, color);
    // Left & Right (adjusted height)
    fb_fill_rect(fb, x, y + thick, thick, h - thick*2, color);
    fb_fill_rect(fb, x + w - thick, y + thick, thick, h - thick*2, color);
}

// ----------------------------------------------------------------------------
//  Tree Logic (Fixed Resource Management)
// ----------------------------------------------------------------------------

// Helper to return a node to the pool
void tree_free(tree_node_t *node) {
    if (!node) return;

    int idx = node - nodes;
    if (idx < 0 || idx >= MAX_WINDOWS * 2)
        return;

    if (free_stack_top + 1 >= MAX_WINDOWS * 2) {
        sys_write(0, "tree_free: free_stack overflow\n");
    }

    // Optional: catch double-free
    if (node->parent == (void*)0xDEADDEAD) {
        sys_write(0, "tree_free: double free\n");
    }

    node->parent = (void*)0xDEADDEAD;
    free_stack[++free_stack_top] = idx;
}

tree_node_t* tree_alloc(void) {
    int idx = -1;

    // 1. Try to recycle a freed node first
    if (free_stack_top >= 0) {
        idx = free_stack[free_stack_top--];
    } 
    // 2. Otherwise allocate a new one from the linear pool
    else if (node_pool_idx < (MAX_WINDOWS * 2)) {
        idx = node_pool_idx++;
    }

    // 3. If both fail, we are truly out of memory
    if (idx == -1) return NULL;

    memset(&nodes[idx], 0, sizeof(tree_node_t));
    return &nodes[idx];
}

tree_node_t* get_sibling(tree_node_t* node) {
    if (!node || !node->parent) return NULL;
    if (node->parent->left == node) return node->parent->right;
    return node->parent->left;
}

void wm_insert_window_into_tree(window_t *win) {
    tree_node_t *new_leaf = tree_alloc();
    if (!new_leaf) return; // Prevent crash if full

    new_leaf->is_leaf = 1; new_leaf->win = win; new_leaf->parent = NULL; 
    if (!root) { root = new_leaf; focused_node = new_leaf; return; }
    
    tree_node_t *target = focused_node ? focused_node : root;
    while (!target->is_leaf) target = target->left;
    
    tree_node_t *container = tree_alloc();
    if (!container) return; // Prevent crash if full

    container->is_leaf = 0; container->split_dir = next_split_mode; container->parent = target->parent;
    if (target->parent) {
        if (target->parent->left == target) target->parent->left = container;
        else target->parent->right = container;
    } else { root = container; }
    
    container->left = target; target->parent = container;
    container->right = new_leaf; new_leaf->parent = container;
    focused_node = new_leaf;
}

void wm_delete_node(tree_node_t *node) {
    if (!node) return;
    
    if (node->is_leaf && node->win) {
        window_t *w = node->win;
        
        // If we have a valid pointer and it's not the initial framebuffer
        if (w->buffer_wm_ptr) {
            uint64_t size = w->width * w->height * 4;
            sys_unmap(w->buffer_wm_ptr, size);
            
            w->buffer_wm_ptr = NULL; // Safety
        }
    }
    // --------------------------

    tree_node_t *parent = node->parent;

    // Case 1: Removing the Root (Last window)
    if (!parent) {
        if (node == root) root = NULL;
        focused_node = NULL;
        tree_free(node); // Recycle root
        return;
    }

    // Case 2: Removing a child
    tree_node_t *sibling = get_sibling(node);
    tree_node_t *grandparent = parent->parent;

    if (sibling) {
        sibling->parent = grandparent;
        if (grandparent) {
            if (grandparent->left == parent) grandparent->left = sibling;
            else grandparent->right = sibling;
        } else { 
            root = sibling; 
        }
    }

    // Update focus
    if (focused_node == node) {
        tree_node_t *candidate = sibling;
        while (candidate && !candidate->is_leaf) candidate = candidate->left;
        focused_node = candidate;
    }

    // FIX: Recycle the memory!
    tree_free(node);   // Free the window leaf
    tree_free(parent); // Free the container (split) node
}

// ----------------------------------------------------------------------------
//  Layout & Render (Optimized)
// ----------------------------------------------------------------------------

void layout_recursive(tree_node_t *node, int x, int y, int w, int h) {
    if (!node) return;
    node->x = x; node->y = y; node->w = w; node->h = h;

    if (node->is_leaf) {
        if (node->win) {
            window_t *win = node->win;
            int old_w = win->width; int old_h = win->height;
            win->x = x; win->y = y; win->width = w; win->height = h;
            if (old_w != w || old_h != h) {
                sys_ipc_send(win->pid, MSG_WINDOW_RESIZE, w, h, 0);
            }
        }
        return;
    }

    int half_w = (node->split_dir == SPLIT_V) ? w / 2 : w; 
    int half_h = (node->split_dir == SPLIT_H) ? h / 2 : h; 
    int next_x = (node->split_dir == SPLIT_V) ? x + half_w : x;
    int next_y = (node->split_dir == SPLIT_H) ? y + half_h : y;
    int rem_w = (node->split_dir == SPLIT_V) ? w - half_w : w;
    int rem_h = (node->split_dir == SPLIT_H) ? h - half_h : h;

    layout_recursive(node->left, x, y, half_w, half_h);
    layout_recursive(node->right, next_x, next_y, rem_w, rem_h);
}

void wm_update_layout(void) {
    if (root) layout_recursive(root, 0, 0, framebuffer.fb_width, framebuffer.fb_height);
}

void render_recursive(tree_node_t *node, uint32_t *fb) {
    if (!node) return;

    if (node->is_leaf && node->win && node->win->alive) {
        window_t *win = node->win;
        uint32_t border_col = (node == focused_node) ? 0xff585b70 : 0xff313244;
        fb_draw_border(fb, win->x, win->y, win->width, win->height, border_col, 4);

        int b = 4;
        int cw = win->width - (b*2);
        int ch = win->height - (b*2);

        if (cw > 0 && ch > 0 && win->buffer_wm_ptr) {
            uint8_t *dest_ptr = (uint8_t*)fb + ((win->y + b) * screen_pitch_bytes) + ((win->x + b) * 4);
            uint8_t *src_ptr = (uint8_t*)win->buffer_wm_ptr;
            int row_len = cw * 4;
            int client_pitch = win->width * 4;

            for (int i = 0; i < ch; i++) {
                memcpy(dest_ptr, src_ptr, row_len);
                dest_ptr += screen_pitch_bytes;
                src_ptr += client_pitch;
            }
        }
    } else {
        render_recursive(node->left, fb);
        render_recursive(node->right, fb);
    }
}

void wm_draw(void) {
    // Clear Background
    memset(back_buffer, 0x20, back_buffer_size); // 0x20202020 dark grey
    render_recursive(root, back_buffer);
}

void wm_present(void) {
    memcpy((void*)framebuffer.fb_addr, back_buffer, back_buffer_size);
}

// ----------------------------------------------------------------------------
//  Window Management APIs
// ----------------------------------------------------------------------------

void wm_refresh_window(int pid, int req_x, int req_y, int req_w, int req_h) {
    window_t *win = NULL;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].pid == pid && windows[i].alive) { win = &windows[i]; break; }
    }
    if (!win || win->width <= 0 || win->height <= 0 || !win->buffer_wm_ptr) return;

    int border = 4;
    int inner_w = win->width - (border * 2);
    int inner_h = win->height - (border * 2);

    // Normalize request
    int rx = (req_w <= 0) ? 0 : req_x;
    int ry = (req_h <= 0) ? 0 : req_y;
    int rw = (req_w <= 0) ? inner_w : req_w;
    int rh = (req_h <= 0) ? inner_h : req_h;

    // Clip against inner window bounds
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > inner_w) rw = inner_w - rx;
    if (ry + rh > inner_h) rh = inner_h - ry;
    if (rw <= 0 || rh <= 0) return;

    // --- OPTIMIZED COPY LOOP ---
    int screen_x = win->x + border + rx;
    int screen_y = win->y + border + ry;
    
    // Pointers in BYTES
    uint8_t *dst = (uint8_t*)back_buffer + (screen_y * screen_pitch_bytes) + (screen_x * 4);
    uint8_t *src = (uint8_t*)win->buffer_wm_ptr + (ry * win->width * 4) + (rx * 4);
    
    int row_copy_bytes = rw * 4;
    int src_stride = win->width * 4;

    for (int i = 0; i < rh; i++) {
        memcpy(dst, src, row_copy_bytes);
        dst += screen_pitch_bytes;
        src += src_stride;
    }

    wm_present_rect(screen_x, screen_y, rw, rh);
}

void wm_handle_new_client(int pid) {
    int slot = -1;
    for(int i=0; i<MAX_WINDOWS; i++) if (!windows[i].alive) { slot = i; break; }
    if (slot == -1) return;
    window_t *win = &windows[slot];
    
    uint64_t buffer_size = framebuffer.fb_width * framebuffer.fb_height * 4;
    uint64_t client_virt_addr;
    void* local_ptr = sys_share_mem(pid, buffer_size, &client_virt_addr);
    if (!local_ptr) return;

    win->id = slot; win->pid = pid; win->alive = 1;
    win->buffer_wm_ptr = (uint32_t*)local_ptr;
    win->buffer_client_ptr = client_virt_addr;

    wm_insert_window_into_tree(win);
    wm_update_layout();
    sys_ipc_send(pid, MSG_WINDOW_CREATED, win->width, win->height, win->buffer_client_ptr);
}

// Key handling helpers
int collect_leaves(tree_node_t *node, tree_node_t **list, int count) {
    if (!node) return count;
    if (node->is_leaf) { list[count++] = node; return count; }
    count = collect_leaves(node->left, list, count);
    return collect_leaves(node->right, list, count);
}

void wm_focus_next(void) {
    if (!root) return;
    tree_node_t *leaves[MAX_WINDOWS];
    int count = collect_leaves(root, leaves, 0);
    if (count == 0) return;
    int current_idx = -1;
    for (int i = 0; i < count; i++) if (leaves[i] == focused_node) { current_idx = i; break; }
    focused_node = leaves[(current_idx + 1) % count];
}

void wm_kill_focused(void) {
    if (!focused_node || !focused_node->win) return;
    
    // 1. Notify the process it needs to die
    sys_ipc_send(focused_node->win->pid, MSG_QUIT_REQUEST, 0, 0, 0);

    // 2. Mark window struct as dead
    focused_node->win->alive = 0;
    
    // 3. Remove from tree structure
    wm_delete_node(focused_node);
    
    // 4. Recalculate layout and Redraw
    wm_update_layout();
    wm_draw();
    wm_present();
}

void wm_handle_input(uint16_t key_packet) {
    char key_char = (char)(key_packet & 0xFF);
    int is_alt = (key_packet & 0x0400); 

    if (is_alt) {
        if (key_char == '\n') {
            char* term_argv[] = {
                "/",                // argv[0]: CWD (Root)
                "/bin/idpterm.elf",   // argv[1]: Program Name
                NULL                // Null terminator
            };
            int term_argc = 2;
            sys_exec("/bin/idpterm.elf", term_argc, term_argv); 
            return; 
        }
        if (key_char == 'h') { next_split_mode = SPLIT_H; return; }
        if (key_char == 'v') { next_split_mode = SPLIT_V; return; }
        if (key_char == '\t') { wm_focus_next(); wm_draw(); wm_present(); return; } 
        if (key_char == 'q') { wm_kill_focused(); wm_draw(); wm_present(); return; }
        return;
    }
    if (focused_node && focused_node->win && focused_node->win->alive) {
        sys_ipc_send(focused_node->win->pid, MSG_KEY_EVENT, key_packet, 0, 0);
    }
}

// ----------------------------------------------------------------------------
//  Main Loop
// ----------------------------------------------------------------------------

void _start() {
    if (sys_get_framebuffer_info(&framebuffer) != 0) sys_exit(1);

    screen_pitch_bytes = framebuffer.fb_pitch;
    back_buffer_size = screen_pitch_bytes * framebuffer.fb_height;
    back_buffer = (uint32_t*)malloc(back_buffer_size);
    if (!back_buffer) sys_exit(1);

    for(int i=0; i<MAX_WINDOWS; i++) reset_dirty(i);

    fb_fill_rect(back_buffer, 0, 0, framebuffer.fb_width, framebuffer.fb_height, 0xFF202020);
    wm_present_rect(0, 0, framebuffer.fb_width, framebuffer.fb_height);

    message_t msg;
    for (;;) {
        int work_done = 0;

        // --- PHASE 1: Process Messages (Batching) ---
        while (sys_ipc_recv(&msg) == 0) {
            work_done = 1;
            
            if (msg.type == MSG_REQUEST_WINDOW) {
                wm_handle_new_client(msg.sender_pid);
                wm_draw(); wm_present(); 
            }
            else if (msg.type == MSG_BUFFER_UPDATE) {
                int slot = -1;
                for(int i=0; i<MAX_WINDOWS; i++) {
                    if (windows[i].pid == msg.sender_pid && windows[i].alive) { 
                        slot = i; break; 
                    }
                }

                if (slot != -1) {
                    int u_x = (int)(msg.data1 >> 32);
                    int u_y = (int)(msg.data1 & 0xFFFFFFFF);
                    int u_w = (int)(msg.data2 >> 32);
                    int u_h = (int)(msg.data2 & 0xFFFFFFFF);
                    mark_dirty(slot, u_x, u_y, u_w, u_h);
                }
            }
        }

        // --- PHASE 2: Input ---
        uint16_t key = sys_read_key();
        if (key > 0) {
            work_done = 1;
            wm_handle_input(key);
        }

        // --- PHASE 3: Deferred Rendering ---
        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (dirty_map[i].is_dirty && windows[i].alive) {
                int rect_w = dirty_map[i].max_x - dirty_map[i].min_x;
                int rect_h = dirty_map[i].max_y - dirty_map[i].min_y;

                wm_refresh_window(windows[i].pid, 
                                  dirty_map[i].min_x, dirty_map[i].min_y, 
                                  rect_w, rect_h);
                
                reset_dirty(i);
                work_done = 1;
            }
        }

        if (!work_done) {
            asm volatile("pause");
        }
    }
}