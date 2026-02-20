/* date = February 17th 2026 2:38 pm */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

#define CTRL_MASK  (1 << 0)
#define ALT_MASK   (1 << 1)
#define SHIFT_MASK (1 << 2)
#define CAPS_MASK  (1 << 3)

typedef enum {
    KEY_UNKNOWN = 0,

    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H,
    KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P,
    KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X,
    KEY_Y, KEY_Z,

    KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,

    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, 
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,

    KEY_MINUS,      
    KEY_EQUAL,      
    KEY_LBRACKET,   
    KEY_RBRACKET,   
    KEY_SEMICOLON,  
    KEY_QUOTE,      
    KEY_BACKTICK,   
    KEY_BACKSLASH,  
    KEY_COMMA,      
    KEY_DOT,        
    KEY_SLASH,      

    KEY_BACKSPACE,
    KEY_ENTER,
    KEY_TAB,
    KEY_SPACE,
    KEY_ESC,
    
    KEY_LSHIFT, KEY_RSHIFT,
    KEY_LCTRL,  KEY_RCTRL,
    KEY_LALT,   KEY_RALT,
    
    KEY_CAPSLOCK,
    KEY_NUMLOCK,
    KEY_SCROLLLOCK,

    KEY_INSERT, KEY_DELETE,
    KEY_HOME,   KEY_END,
    KEY_PGUP,   KEY_PGDN,
    KEY_UP,     KEY_DOWN, 
    KEY_LEFT,   KEY_RIGHT,

    KEY_KP_0, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4,
    KEY_KP_5, KEY_KP_6, KEY_KP_7, KEY_KP_8, KEY_KP_9,
    KEY_KP_ADD, KEY_KP_SUB, KEY_KP_MUL, KEY_KP_DIV, 
    KEY_KP_DECIMAL, KEY_KP_ENTER

} kernel_scancode_t;

typedef struct key_event {
    uint8_t code;
    uint8_t status_mask;
    bool is_pressed;
} key_event_t;

typedef enum {
    NORMAL_STATE,
    PREFIX_STATE
} kb_state_t;

void keyboard_init();
void keyboard_driver_irq_handler();
bool keyboard_poll();
key_event_t keyboard_read();

#endif
