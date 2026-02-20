#include <drivers/keyboard.h>
#include <drivers/apic.h>
#include <utility/port.h>
#include <utility/kstring.h>

#define BUFFER_SIZE 255
#define KB_DATA_PORT 0x60

static kernel_scancode_t scancode_mapping[] = {
    KEY_UNKNOWN, KEY_ESC, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, 
    KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL, KEY_BACKSPACE, KEY_TAB,
    
    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, 
    KEY_O, KEY_P, KEY_LBRACKET, KEY_RBRACKET, KEY_ENTER, KEY_LCTRL, KEY_A, KEY_S,
    
    KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, 
    KEY_QUOTE, KEY_BACKTICK, KEY_LSHIFT, KEY_BACKSLASH, KEY_Z, KEY_X, KEY_C, KEY_V,
    
    KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_RSHIFT, KEY_KP_MUL,
    KEY_LALT, KEY_SPACE, KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
    
    KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_NUMLOCK, KEY_SCROLLLOCK, KEY_HOME,
    KEY_UP, KEY_PGUP, KEY_KP_SUB, KEY_LEFT, KEY_KP_5, KEY_RIGHT, KEY_KP_ADD, KEY_END,
    
    KEY_DOWN, KEY_PGDN, KEY_INSERT, KEY_DELETE, KEY_UNKNOWN, KEY_UNKNOWN, KEY_UNKNOWN, KEY_F11,
    KEY_F12
};

key_event_t kb_buffer[BUFFER_SIZE];
kb_state_t state;
uint8_t current_modifiers;
uint8_t buf_write_pos;
uint8_t buf_read_pos;

void keyboard_init() {
    io_apic_map_irq(1, 0x21);
    memset(kb_buffer, 0, sizeof(key_event_t) * BUFFER_SIZE);
    state = NORMAL_STATE;
    buf_write_pos = 0;
    buf_read_pos = 0;
    current_modifiers = 0;
}

void update_modifiers(kernel_scancode_t k_code, bool is_pressed) {
    uint8_t mask = 0;
    
    if (k_code == KEY_LCTRL || k_code == KEY_RCTRL) mask = CTRL_MASK;
    else if (k_code == KEY_LALT || k_code == KEY_RALT) mask = ALT_MASK;
    else if (k_code == KEY_LSHIFT || k_code == KEY_RSHIFT) mask = SHIFT_MASK;
    
    if (mask) {
        if (is_pressed) current_modifiers |= mask;
        else current_modifiers &= ~mask;
    }
}

void keyboard_driver_irq_handler() {
    uint8_t scancode = inportb(KB_DATA_PORT);
    if (scancode == 0xE0) {
        state = PREFIX_STATE;
        return;
    }

    bool is_break = scancode & 0x80;
    uint8_t base_scancode = scancode & 0x7F;

    if (state == PREFIX_STATE) {
        state = NORMAL_STATE;
    }

    if (base_scancode >= sizeof(scancode_mapping) / sizeof(kernel_scancode_t)) {
        return; 
    }

    kernel_scancode_t k_code = scancode_mapping[base_scancode];
    if (k_code == KEY_UNKNOWN) return;

    update_modifiers(k_code, !is_break);

    if (!is_break) {
        key_event_t event;
        event.code = k_code;
        event.status_mask = current_modifiers;
        event.is_pressed = true;

        uint8_t next_pos = (buf_write_pos + 1) % BUFFER_SIZE;
        if (next_pos != buf_read_pos) { 
            kb_buffer[buf_write_pos] = event;
            buf_write_pos = next_pos;
        }
    }
}

bool keyboard_poll() {
    return buf_write_pos != buf_read_pos;
}

key_event_t keyboard_read() {
    key_event_t event = {0};
    if (buf_write_pos != buf_read_pos) {
        event = kb_buffer[buf_read_pos];
        buf_read_pos = (buf_read_pos + 1) % BUFFER_SIZE;
    }
    return event;
}