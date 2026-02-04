#include <keyboard.h>

static uint16_t keyboard_ring_buffer[KEYBOARD_RING_BUFFER_SIZE];
static volatile uint16_t kb_head = 0;
static volatile uint16_t kb_tail = 0;

static int mod_shift = 0;
static int mod_ctrl  = 0;
static int mod_alt   = 0;
static int mod_super = 0;

static int e0_prefix = 0; // Extended scan code state

static char scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
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

    serial_printf("Keyboard driver installed (PIC 1 unmasked)\n");
}

void keyboard_buffer_write(uint16_t data) {
    uint16_t next_head = (kb_head + 1) % KEYBOARD_RING_BUFFER_SIZE;
    
    if (next_head != kb_tail) {
        keyboard_ring_buffer[kb_head] = data;
        kb_head = next_head;
    }
}

uint16_t keyboard_read_key(void) {
    if (kb_head == kb_tail) {
        return 0;
    }
    
    uint16_t val = keyboard_ring_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KEYBOARD_RING_BUFFER_SIZE;
    return val;
}

void keyboard_handler(void) {
    uint8_t status = inb(KEYBOARD_STATUS_PORT);

    if (status & 0x01) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);

        // Handle Extended Byte (E0)
        // Sent before Right-Ctrl, Right-Alt, Keypad, and Super keys
        if (scancode == 0xE0) {
            e0_prefix = 1;
            return;
        }

        // Handle Modifiers (Press & Release)
        // --- SHIFT ---
        if (scancode == 0x2A || scancode == 0x36) { mod_shift = 1; e0_prefix = 0; return; }
        if (scancode == 0xAA || scancode == 0xB6) { mod_shift = 0; e0_prefix = 0; return; }

        // --- CTRL (0x1D) ---
        if (scancode == 0x1D) { mod_ctrl = 1; e0_prefix = 0; return; }
        if (scancode == 0x9D) { mod_ctrl = 0; e0_prefix = 0; return; }

        // --- ALT (0x38) ---
        if (scancode == 0x38) { mod_alt = 1; e0_prefix = 0; return; }
        if (scancode == 0xB8) { mod_alt = 0; e0_prefix = 0; return; }

        // --- SUPER / META (E0 5B) ---
        if (e0_prefix && scancode == 0x5B) { mod_super = 1; e0_prefix = 0; return; }
        if (e0_prefix && scancode == 0xDB) { mod_super = 0; e0_prefix = 0; return; }

        // Handle Regular Keys
        if (!(scancode & 0x80)) { // If Press
            // Prevent E0 prefix from messing up standard map lookups
            // (e.g., E0 5B is Super, but 5B alone is '[')
            if (e0_prefix) {
                // If we are here, we got an E0 extended code that isn't a known modifier.
                // For now, ignore extended keys (like Keypad Enter) to avoid garbage chars.
                e0_prefix = 0;
                return;
            }

            if (scancode < 128 && scancode_map[scancode] != 0) {
                char ascii = 0;
                if (mod_shift) {
                    ascii = scancode_map_shifted[scancode];
                } else {
                    ascii = scancode_map[scancode];
                }

                // Pack Modifier Flags + ASCII into 16 bits
                uint16_t packet = ascii;
                if (mod_shift) packet |= KEY_MOD_SHIFT;
                if (mod_ctrl)  packet |= KEY_MOD_CTRL;
                if (mod_alt)   packet |= KEY_MOD_ALT;
                if (mod_super) packet |= KEY_MOD_SUPER;

                keyboard_buffer_write(packet);
            }
        }
        
        // Reset prefix state for next interrupt
        e0_prefix = 0;
    }
}