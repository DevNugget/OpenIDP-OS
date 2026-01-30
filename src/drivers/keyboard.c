#include <keyboard.h>

static char keyboard_ring_buffer[KEYBOARD_RING_BUFFER_SIZE];
static volatile uint16_t kb_head = 0;
static volatile uint16_t kb_tail = 0;

static int shift_active = 0;

// Scan Code Set 1 (US QWERTY)
// 0 means no ASCII representation (like Shift, Ctrl, F1, etc.)
static char scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // ... extended keys would go here (F-keys, Keypad) ...
};

static char scancode_map_shifted[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

void keyboard_init(void) {
    pic_enable_irq(1);

    serial_printf("Keyboard driver installed (PIC 1 unmasked)");
}

void keyboard_buffer_write(char c) {
    uint16_t next_head = (kb_head + 1) % KEYBOARD_RING_BUFFER_SIZE;
    
    // If buffer is full, discard the character (or overwrite oldest)
    if (next_head != kb_tail) {
        keyboard_ring_buffer[kb_head] = c;
        kb_head = next_head;
    }
}

char keyboard_read_char(void) {
    // If buffer is empty, return 0
    if (kb_head == kb_tail) {
        return 0;
    }
    
    char c = keyboard_ring_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KEYBOARD_RING_BUFFER_SIZE;
    return c;
}

void keyboard_handler(void) {
    // Read Status Register
    uint8_t status = inb(KEYBOARD_STATUS_PORT);

    // Check if Output Buffer is full (Bit 0 set)
    if (status & 0x01) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);

        // Left Shift Press (0x2A) or Right Shift Press (0x36)
        if (scancode == 0x2A || scancode == 0x36) {
            shift_active = 1;
            return;
        }

        // Left Shift Release (0xAA) or Right Shift Release (0xB6)
        // (Release code is Press Code | 0x80)
        if (scancode == 0xAA || scancode == 0xB6) {
            shift_active = 0;
            return;
        }
        
        // If the top bit is set (0x80), it's a "Key Release" event
        if (scancode & 0x80) {
            // For now, we ignore key releases (except for Shift/Ctrl logic later)
        } else {
            // It's a "Key Press"
            if (scancode < 128 && scancode_map[scancode] != 0) {
                char c = 0;

                // Pick the map based on Shift state
                if (shift_active) {
                    c = scancode_map_shifted[scancode];
                } else {
                    c = scancode_map[scancode];
                }

                keyboard_buffer_write(c);
                
                // Optional: Echo to serial for debugging
                // serial_printf("%c", c); 
            }
        }
    }
}