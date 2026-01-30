//
typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;

// --- Syscall Wrappers ---
void syscall_5args(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    asm volatile (
        "mov %5, %%r8\n"
        "int $0x80"
        : 
        : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3), "c"(arg4), "r"(arg5)
        : "r8", "memory"
    );
}

uint64_t syscall_1arg(uint64_t number, uint64_t arg1) {
    uint64_t ret;
    asm volatile ("int $0x80" : "=a"(ret) : "a"(number), "D"(arg1) : "memory");
    return ret;
}

uint64_t syscall_0arg(uint64_t number) {
    uint64_t ret;
    asm volatile ("int $0x80" : "=a"(ret) : "a"(number) : "memory");
    return ret;
}

#define SYS_READ_KEY 2
#define SYS_GET_SCREEN_PROP 3
#define SYS_DRAW_RECT 4
#define SYS_BLIT 5

// --- Window Manager Logic ---

#define MAX_NODES 64
#define BORDER_WIDTH 4

// Catppuccin-inspired Palette
#define BG_COLOR 0x00181825
#define WIN_COLOR 0x00181825
#define BORDER_COLOR 0x00494d64
#define FOCUS_BORDER_COLOR 0x007f849c

#define MAX_W 1920
#define MAX_H 1080

int screen_w, screen_h;
static uint32_t backbuffer[MAX_W * MAX_H];

typedef struct {
    int id;
    int active;
    int x, y, w, h;
    int color;
    int left_child_idx;   
    int right_child_idx; 
    int parent_idx;
    int split_vertical; // 1 if split vertically, 0 if horizontal
} Node;

Node nodes[MAX_NODES];
int node_count = 0;
int focused_node_idx = 0; // Track the focused window

void init_wm() {
    screen_w = syscall_1arg(SYS_GET_SCREEN_PROP, 0);
    screen_h = syscall_1arg(SYS_GET_SCREEN_PROP, 1);
    
    // Root Node
    nodes[0].id = 0;
    nodes[0].active = 1;
    nodes[0].x = 0;
    nodes[0].y = 0;
    nodes[0].w = screen_w;
    nodes[0].h = screen_h;
    nodes[0].color = WIN_COLOR;
    nodes[0].left_child_idx = -1;
    nodes[0].right_child_idx = -1;
    nodes[0].parent_idx = -1;
    nodes[0].split_vertical = 0;
    
    node_count = 1;
    focused_node_idx = 0;
}

void draw_rect_buffered(int x, int y, int w, int h, int color) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > screen_w) w = screen_w - x;
    if (y + h > screen_h) h = screen_h - y;

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int idx = ((y + j) * screen_w) + (x + i);
            backbuffer[idx] = color;
        }
    }
}

void swap_buffers() {
    syscall_1arg(SYS_BLIT, (uint64_t)backbuffer);
}

// Recalculate geometry recursively (used after closing a window)
void relayout(int idx, int x, int y, int w, int h) {
    if (idx == -1 || !nodes[idx].active) return;

    nodes[idx].x = x;
    nodes[idx].y = y;
    nodes[idx].w = w;
    nodes[idx].h = h;

    if (nodes[idx].left_child_idx != -1) {
        int left = nodes[idx].left_child_idx;
        int right = nodes[idx].right_child_idx;

        if (nodes[idx].split_vertical) {
            int half_w = w / 2;
            relayout(left, x, y, half_w, h);
            relayout(right, x + half_w, y, w - half_w, h);
        } else {
            int half_h = h / 2;
            relayout(left, x, y, w, half_h);
            relayout(right, x, y + half_h, w, h - half_h);
        }
    }
}

void split_window() {
    if (node_count + 2 >= MAX_NODES) return;

    // 1. Target the currently focused window
    int target_idx = focused_node_idx;
    Node* target = &nodes[target_idx];

    // If target is somehow internal (not leaf), abort or find leaf
    if (target->left_child_idx != -1) return; 

    int new_left_idx = node_count++;
    int new_right_idx = node_count++;

    // 2. Decide split direction
    int split_vertical = (target->w > target->h);
    target->split_vertical = split_vertical;

    // 3. Create Left Child (Original Content)
    nodes[new_left_idx] = *target; 
    nodes[new_left_idx].id = new_left_idx;
    nodes[new_left_idx].parent_idx = target_idx;
    nodes[new_left_idx].left_child_idx = -1;
    nodes[new_left_idx].right_child_idx = -1;
    nodes[new_left_idx].color = WIN_COLOR; // Standard Color

    // 4. Create Right Child (New Empty Window)
    nodes[new_right_idx] = nodes[new_left_idx];
    nodes[new_right_idx].id = new_right_idx;
    nodes[new_right_idx].color = WIN_COLOR; // Standard Color

    // 5. Connect to Parent
    target->left_child_idx = new_left_idx;
    target->right_child_idx = new_right_idx;

    // 6. Calculate Geometry
    relayout(target_idx, target->x, target->y, target->w, target->h);

    // 7. Focus the new window
    focused_node_idx = new_right_idx;
}

void close_window() {
    // Cannot close the root if it's the only one
    if (focused_node_idx == 0 && nodes[0].left_child_idx == -1) return;

    int target_idx = focused_node_idx;
    int parent_idx = nodes[target_idx].parent_idx;

    // If we are somehow at root but have children (shouldn't happen with focus logic), abort
    if (parent_idx == -1) return;

    Node* parent = &nodes[parent_idx];
    
    // Identify sibling
    int sibling_idx = (parent->left_child_idx == target_idx) 
                      ? parent->right_child_idx 
                      : parent->left_child_idx;
    
    Node* sibling = &nodes[sibling_idx];

    // --- PROMOTE SIBLING TO PARENT ---
    // We copy the sibling's data into the parent slot.
    // This effectively deletes the parent node and the target node,
    // replacing the parent's space with the sibling.

    // 1. Save parent's geometry (The space we want to fill)
    int px = parent->x;
    int py = parent->y;
    int pw = parent->w;
    int ph = parent->h;
    int p_parent = parent->parent_idx;

    // 2. Overwrite parent with sibling data
    *parent = *sibling;
    
    // 3. Restore parent's identity bits
    parent->id = parent_idx;
    parent->parent_idx = p_parent;
    parent->x = px;
    parent->y = py;
    parent->w = pw;
    parent->h = ph;

    // 4. If the sibling had children, we must update their parent pointer
    // because "sibling" node is gone, they are now children of "parent"
    if (parent->left_child_idx != -1) {
        nodes[parent->left_child_idx].parent_idx = parent_idx;
        nodes[parent->right_child_idx].parent_idx = parent_idx;
    }

    // 5. Deactivate the old nodes to allow reuse (optional, strict cleanup)
    nodes[target_idx].active = 0;
    nodes[sibling_idx].active = 0;

    // 6. Recalculate geometry for the promoted tree
    relayout(parent_idx, px, py, pw, ph);

    // 7. Fix focus
    focused_node_idx = parent_idx;
    // If the promoted node is a container, focus a leaf inside it
    while (nodes[focused_node_idx].left_child_idx != -1) {
        focused_node_idx = nodes[focused_node_idx].left_child_idx;
    }
}

void cycle_focus() {
    int start = focused_node_idx;
    int curr = start;
    
    // Loop through nodes array to find next active leaf
    do {
        curr = (curr + 1) % MAX_NODES;
        if (nodes[curr].active && nodes[curr].left_child_idx == -1) {
            focused_node_idx = curr;
            return;
        }
    } while (curr != start);
}

void render_tree(int idx) {
    if (idx == -1 || !nodes[idx].active) return;

    if (nodes[idx].left_child_idx == -1) {
        // Determine border color based on focus
        int border_col = (idx == focused_node_idx) ? FOCUS_BORDER_COLOR : BORDER_COLOR;

        // Draw Border
        draw_rect_buffered(nodes[idx].x, nodes[idx].y, nodes[idx].w, nodes[idx].h, border_col);
        
        // Draw Content
        draw_rect_buffered(
            nodes[idx].x + BORDER_WIDTH, 
            nodes[idx].y + BORDER_WIDTH, 
            nodes[idx].w - (BORDER_WIDTH * 2), 
            nodes[idx].h - (BORDER_WIDTH * 2), 
            nodes[idx].color
        );
    } else {
        render_tree(nodes[idx].left_child_idx);
        render_tree(nodes[idx].right_child_idx);
    }
}

void _start() {
    init_wm();

    // Initial Draw
    draw_rect_buffered(0, 0, screen_w, screen_h, BG_COLOR);
    render_tree(0);
    swap_buffers();

    while(1) {
        uint64_t key = syscall_0arg(SYS_READ_KEY);
        int dirty = 0;

        if (key > 0) {
            if (key == 'n') {
                split_window();
                dirty = 1;
            }
            else if (key == 'x') { // Close
                close_window();
                dirty = 1;
            }
            else if (key == '\t') { // Tab
                cycle_focus();
                dirty = 1;
            }
            else if (key == 'q') {
                syscall_1arg(1, 0); 
            }
        }

        if (dirty) {
            draw_rect_buffered(0, 0, screen_w, screen_h, BG_COLOR);
            render_tree(0);
            swap_buffers();
        }
    }
}