#include "idpwm.h"
#include "openidp.h"
#include "libc/heap.h"
#include "libc/string.h"

// --- Definitions & Helpers ---

#ifndef NULL
#define NULL ((void*)0)
#endif

// Tree splitting directions
typedef enum { SPLIT_H, SPLIT_V } split_dir_t;

// The Node Structure (Everything is a Node)
typedef struct tree_node {
    struct tree_node *parent;
    struct tree_node *left;  // Child A
    struct tree_node *right; // Child B
    
    int is_leaf;
    split_dir_t split_dir;   // How children are arranged (if container)
    
    // Leaf Data
    window_t *win;           
    
    // Layout State (Calculated every frame)
    int x, y, w, h;
} tree_node_t;

// --- Globals ---

static window_t windows[MAX_WINDOWS];
static tree_node_t nodes[MAX_WINDOWS * 2]; // Pool for nodes (leaves + containers)
static int node_pool_idx = 0;

static tree_node_t *root = NULL;
static tree_node_t *focused_node = NULL;
static split_dir_t next_split_mode = SPLIT_V;

static uint32_t *back_buffer;
static int back_buffer_size;

// --- Prototypes ---

tree_node_t* tree_alloc(void);
void wm_update_layout(void);
void wm_render(void);
void wm_focus_next(void);
void wm_kill_focused(void);

// ----------------------------------------------------------------------------
//  Graphics Helpers
// ----------------------------------------------------------------------------

void fb_fill_rect(uint32_t* fb_ptr, int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > framebuffer.fb_width) w = framebuffer.fb_width - x;
    if (y + h > framebuffer.fb_height) h = framebuffer.fb_height - y;

    for (int yy = 0; yy < h; yy++) {
        uint32_t *row = fb_ptr + (y + yy) * (framebuffer.fb_pitch / 4);
        for (int xx = 0; xx < w; xx++) row[x + xx] = color;
    }
}

void fb_draw_border(uint32_t* fb, int x, int y, int w, int h, uint32_t color, int thick) {
    if (w < thick*2 || h < thick*2) return;
    fb_fill_rect(fb, x, y, w, thick, color);              // Top
    fb_fill_rect(fb, x, y + h - thick, w, thick, color);  // Bottom
    fb_fill_rect(fb, x, y, thick, h, color);              // Left
    fb_fill_rect(fb, x + w - thick, y, thick, h, color);  // Right
}

// ----------------------------------------------------------------------------
//  Tree Logic: The Core Tiling Engine
// ----------------------------------------------------------------------------

tree_node_t* tree_alloc(void) {
    // Simple bump allocator for nodes. In a long-running OS, implement a free list.
    if (node_pool_idx >= MAX_WINDOWS * 2) return NULL;
    memset(&nodes[node_pool_idx], 0, sizeof(tree_node_t));
    return &nodes[node_pool_idx++];
}

// Helper to find the sibling of a node
tree_node_t* get_sibling(tree_node_t* node) {
    if (!node || !node->parent) return NULL;
    if (node->parent->left == node) return node->parent->right;
    return node->parent->left;
}

// Insert a new window into the tree relative to the currently focused node
void wm_insert_window_into_tree(window_t *win) {
    tree_node_t *new_leaf = tree_alloc();
    new_leaf->is_leaf = 1;
    new_leaf->win = win;
    new_leaf->parent = NULL; 

    // CASE 1: Empty Tree
    if (!root) {
        root = new_leaf;
        focused_node = new_leaf;
        return;
    }

    // CASE 2: Split the Focused Node
    tree_node_t *target = focused_node;
    if (!target) target = root;

    // We must find a leaf to split. If target is a container (shouldn't happen with valid focus), descend left.
    while (!target->is_leaf) target = target->left;

    // Create a new container to replace the target leaf
    tree_node_t *container = tree_alloc();
    container->is_leaf = 0;
    container->split_dir = next_split_mode; // User selected split
    container->parent = target->parent;

    // Hook container into parent
    if (target->parent) {
        if (target->parent->left == target) target->parent->left = container;
        else target->parent->right = container;
    } else {
        root = container;
    }

    // Attach children to container
    // Existing window -> Left/Top
    container->left = target;
    target->parent = container;

    // New window -> Right/Bottom
    container->right = new_leaf;
    new_leaf->parent = container;

    // Update Focus
    focused_node = new_leaf;
}

// Remove a node and repair the tree
void wm_delete_node(tree_node_t *node) {
    if (!node) return;

    tree_node_t *parent = node->parent;

    // CASE 1: Removing the Root (Last window)
    if (!parent) {
        if (node == root) root = NULL;
        focused_node = NULL;
        // In a real allocator, we would free(node) here
        return;
    }

    // CASE 2: Removing a child of a container
    tree_node_t *sibling = get_sibling(node);
    tree_node_t *grandparent = parent->parent;

    // The sibling promotes to the parent's position (Collapse)
    if (sibling) {
        sibling->parent = grandparent;
        
        if (grandparent) {
            if (grandparent->left == parent) grandparent->left = sibling;
            else grandparent->right = sibling;
        } else {
            // Parent was root, so Sibling becomes new root
            root = sibling;
        }
    }
    
    // Focus Repair: Focus the sibling if we deleted the focused node
    if (focused_node == node) {
        // Find a leaf inside sibling to focus
        tree_node_t *candidate = sibling;
        while (candidate && !candidate->is_leaf) candidate = candidate->left;
        focused_node = candidate;
    }

    // In a real OS, free(node) and free(parent)
}

// ----------------------------------------------------------------------------
//  Layout & Render
// ----------------------------------------------------------------------------

// Recursive layout calculator
void layout_recursive(tree_node_t *node, int x, int y, int w, int h) {
    if (!node) return;

    // Save calculated geometry
    node->x = x; node->y = y; node->w = w; node->h = h;

    if (node->is_leaf) {
        if (node->win) {
            window_t *win = node->win;
            int old_w = win->width;
            int old_h = win->height;

            win->x = x; win->y = y;
            win->width = w; win->height = h;

            // Only notify client if dimensions actually changed
            if (old_w != w || old_h != h) {
                sys_ipc_send(win->pid, MSG_WINDOW_RESIZE, w, h, 0);
            }
        }
        return;
    }

    // --- FIX START ---
    // SPLIT_H now modifies HEIGHT (Horizontal Cut -> Top/Bottom)
    // SPLIT_V now modifies WIDTH  (Vertical Cut   -> Left/Right)

    int half_w = (node->split_dir == SPLIT_V) ? w / 2 : w; 
    int half_h = (node->split_dir == SPLIT_H) ? h / 2 : h; 

    int next_x = (node->split_dir == SPLIT_V) ? x + half_w : x;
    int next_y = (node->split_dir == SPLIT_H) ? y + half_h : y;

    // Adjust remainder to fill pixel gaps
    int rem_w = (node->split_dir == SPLIT_V) ? w - half_w : w;
    int rem_h = (node->split_dir == SPLIT_H) ? h - half_h : h;
    // --- FIX END ---

    layout_recursive(node->left, x, y, half_w, half_h);
    layout_recursive(node->right, next_x, next_y, rem_w, rem_h);
}

void wm_update_layout(void) {
    if (root) {
        layout_recursive(root, 0, 0, framebuffer.fb_width, framebuffer.fb_height);
    } else {
        // No windows, just clear background later
    }
}

void render_recursive(tree_node_t *node, uint32_t *fb) {
    if (!node) return;

    if (node->is_leaf && node->win && node->win->alive) {
        window_t *win = node->win;

        // Draw Border
        uint32_t border_col = (node == focused_node) ? 0xFF007ACC : 0xFF444444;
        fb_draw_border(fb, win->x, win->y, win->width, win->height, border_col, 4);

        // Calculate Client Content Area (Inset by border)
        int b = 4;
        int cx = win->x + b;
        int cy = win->y + b;
        int cw = win->width - (b*2);
        int ch = win->height - (b*2);

        if (cw > 0 && ch > 0 && win->buffer_wm_ptr) {
            for (int ly = 0; ly < ch; ly++) {
                uint32_t* dest = fb + ((cy + ly) * (framebuffer.fb_pitch/4)) + cx;
                uint32_t* src = win->buffer_wm_ptr + (ly * win->width);
                memcpy(dest, src, cw * 4);
            }
        }
    } else {
        render_recursive(node->left, fb);
        render_recursive(node->right, fb);
    }
}

void wm_draw(void) {
    fb_fill_rect(back_buffer, 0, 0, framebuffer.fb_width, framebuffer.fb_height, 0xFF202020);
    render_recursive(root, back_buffer);
}

void wm_present(void) {
    memcpy((void*)framebuffer.fb_addr, back_buffer, back_buffer_size);
}

// ----------------------------------------------------------------------------
//  Window Management APIs
// ----------------------------------------------------------------------------

void wm_handle_new_client(int pid) {
    // 1. Find slot
    int slot = -1;
    for(int i=0; i<MAX_WINDOWS; i++) {
        if (!windows[i].alive) { slot = i; break; }
    }
    if (slot == -1) return;

    window_t *win = &windows[slot];
    
    // 2. Alloc Memory
    uint64_t buffer_size = framebuffer.fb_width * framebuffer.fb_height * 4;
    uint64_t client_virt_addr;
    void* local_ptr = sys_share_mem(pid, buffer_size, &client_virt_addr);
    if (!local_ptr) return;

    // 3. Init Window
    win->id = slot;
    win->pid = pid;
    win->alive = 1;
    win->buffer_wm_ptr = (uint32_t*)local_ptr;
    win->buffer_client_ptr = client_virt_addr;

    // 4. Insert & Layout
    wm_insert_window_into_tree(win);
    wm_update_layout();

    // 5. Notify Client
    sys_ipc_send(pid, MSG_WINDOW_CREATED, win->width, win->height, win->buffer_client_ptr);
}

// Helper to collect leaves into an array for cycling
int collect_leaves(tree_node_t *node, tree_node_t **list, int count) {
    if (!node) return count;
    if (node->is_leaf) {
        list[count++] = node;
        return count;
    }
    count = collect_leaves(node->left, list, count);
    count = collect_leaves(node->right, list, count);
    return count;
}

void wm_focus_next(void) {
    if (!root) return;

    tree_node_t *leaves[MAX_WINDOWS];
    int count = collect_leaves(root, leaves, 0);
    if (count == 0) return;

    // Find current index
    int current_idx = -1;
    for (int i = 0; i < count; i++) {
        if (leaves[i] == focused_node) {
            current_idx = i;
            break;
        }
    }

    // Cycle
    int next_idx = (current_idx + 1) % count;
    focused_node = leaves[next_idx];
}

void wm_kill_focused(void) {
    if (!focused_node || !focused_node->win) return;
    
    // 1. Mark window struct as dead
    focused_node->win->alive = 0;
    // TODO: sys_ipc_send(focused_node->win->pid, MSG_KILL...);
    
    // 2. Remove from tree structure
    wm_delete_node(focused_node);
    
    // 3. Recalculate
    wm_update_layout();
}

// ----------------------------------------------------------------------------
//  Main Loop
// ----------------------------------------------------------------------------

#define KEY_MOD_SUPER  0x0800
#define KEY_MOD_ALT    0x0400
#define KEY_MOD_CTRL   0x0200
#define KEY_MOD_SHIFT  0x0100

void wm_handle_input(uint16_t key_packet) {
    char key_char = (char)(key_packet & 0xFF);
    int is_super  = (key_packet & KEY_MOD_SUPER);
    int is_alt    = (key_packet & KEY_MOD_ALT);

    // 1. WM Global Bindings (Consume the event)
    if (is_alt) {
        if (key_char == '\n') { // Super + Enter
             sys_exec("/bin/terminal.elf");
             return; // Don't pass to window
        }
        if (key_char == 'h') { // Super + H
             next_split_mode = SPLIT_H;
             return;
        }
        if (key_char == 'v') {
            next_split_mode = SPLIT_V;
        }
        if (key_char == '\t') {
            wm_focus_next();
        }
        if (key_char == 'q') { // Super + Q
             wm_kill_focused();
             return;
        }
    }
}

void _start() {
    if (sys_get_framebuffer_info(&framebuffer) != 0) sys_exit(1);

    back_buffer_size = framebuffer.fb_pitch * framebuffer.fb_height;
    back_buffer = (uint32_t*)malloc(back_buffer_size);
    if (!back_buffer) sys_exit(1);

    wm_draw();
    wm_present();

    message_t msg;
    for (;;) {
        int needs_redraw = 0;

        // Process Messages
        while (sys_ipc_recv(&msg) == 0) {
            if (msg.type == MSG_REQUEST_WINDOW) {
                wm_handle_new_client(msg.sender_pid);
                needs_redraw = 1;
            }
            else if (msg.type == MSG_BUFFER_UPDATE) {
                needs_redraw = 1;
            }
        }

        // Process Input
        uint16_t key = sys_read_key();
        if (key > 0) {
            wm_handle_input(key);
            needs_redraw = 1;
        }

        if (needs_redraw) {
            wm_draw();
            wm_present();
        }
    }
}