#include <utility/kascii.h>

static const char kbd_us_lower[256] = {
    [KEY_UNKNOWN] = 0,

    [KEY_A] = 'a', [KEY_B] = 'b', [KEY_C] = 'c', [KEY_D] = 'd', [KEY_E] = 'e',
    [KEY_F] = 'f', [KEY_G] = 'g', [KEY_H] = 'h', [KEY_I] = 'i', [KEY_J] = 'j',
    [KEY_K] = 'k', [KEY_L] = 'l', [KEY_M] = 'm', [KEY_N] = 'n', [KEY_O] = 'o',
    [KEY_P] = 'p', [KEY_Q] = 'q', [KEY_R] = 'r', [KEY_S] = 's', [KEY_T] = 't',
    [KEY_U] = 'u', [KEY_V] = 'v', [KEY_W] = 'w', [KEY_X] = 'x', [KEY_Y] = 'y',
    [KEY_Z] = 'z',

    [KEY_1] = '1', [KEY_2] = '2', [KEY_3] = '3', [KEY_4] = '4', [KEY_5] = '5',
    [KEY_6] = '6', [KEY_7] = '7', [KEY_8] = '8', [KEY_9] = '9', [KEY_0] = '0',

    [KEY_MINUS] = '-',     [KEY_EQUAL] = '=',
    [KEY_LBRACKET] = '[',  [KEY_RBRACKET] = ']',
    [KEY_SEMICOLON] = ';', [KEY_QUOTE] = '\'',
    [KEY_BACKTICK] = '`',  [KEY_BACKSLASH] = '\\',
    [KEY_COMMA] = ',',     [KEY_DOT] = '.',
    [KEY_SLASH] = '/',

    [KEY_BACKSPACE] = '\b',
    [KEY_ENTER] = '\n',
    [KEY_TAB] = '\t',
    [KEY_SPACE] = ' ',
    [KEY_ESC] = 0x1B,

    [KEY_KP_0] = '0', [KEY_KP_1] = '1', [KEY_KP_2] = '2', [KEY_KP_3] = '3',
    [KEY_KP_4] = '4', [KEY_KP_5] = '5', [KEY_KP_6] = '6', [KEY_KP_7] = '7',
    [KEY_KP_8] = '8', [KEY_KP_9] = '9', 
    [KEY_KP_ADD] = '+', [KEY_KP_SUB] = '-', [KEY_KP_MUL] = '*', [KEY_KP_DIV] = '/',
    [KEY_KP_DECIMAL] = '.', [KEY_KP_ENTER] = '\n'
};

static const char kbd_us_shifted[256] = {
    [KEY_UNKNOWN] = 0,

    [KEY_A] = 'A', [KEY_B] = 'B', [KEY_C] = 'C', [KEY_D] = 'D', [KEY_E] = 'E',
    [KEY_F] = 'F', [KEY_G] = 'G', [KEY_H] = 'H', [KEY_I] = 'I', [KEY_J] = 'J',
    [KEY_K] = 'K', [KEY_L] = 'L', [KEY_M] = 'M', [KEY_N] = 'N', [KEY_O] = 'O',
    [KEY_P] = 'P', [KEY_Q] = 'Q', [KEY_R] = 'R', [KEY_S] = 'S', [KEY_T] = 'T',
    [KEY_U] = 'U', [KEY_V] = 'V', [KEY_W] = 'W', [KEY_X] = 'X', [KEY_Y] = 'Y',
    [KEY_Z] = 'Z',

    [KEY_1] = '!', [KEY_2] = '@', [KEY_3] = '#', [KEY_4] = '$', [KEY_5] = '%',
    [KEY_6] = '^', [KEY_7] = '&', [KEY_8] = '*', [KEY_9] = '(', [KEY_0] = ')',

    [KEY_MINUS] = '_',     [KEY_EQUAL] = '+',
    [KEY_LBRACKET] = '{',  [KEY_RBRACKET] = '}',
    [KEY_SEMICOLON] = ':', [KEY_QUOTE] = '"',
    [KEY_BACKTICK] = '~',  [KEY_BACKSLASH] = '|',
    [KEY_COMMA] = '<',     [KEY_DOT] = '>',
    [KEY_SLASH] = '?',

    [KEY_BACKSPACE] = '\b',
    [KEY_ENTER] = '\n',
    [KEY_TAB] = '\t',
    [KEY_SPACE] = ' ',
    [KEY_ESC] = 0x1B,

    [KEY_KP_0] = '0', [KEY_KP_1] = '1', [KEY_KP_2] = '2', [KEY_KP_3] = '3',
    [KEY_KP_4] = '4', [KEY_KP_5] = '5', [KEY_KP_6] = '6', [KEY_KP_7] = '7',
    [KEY_KP_8] = '8', [KEY_KP_9] = '9', 
    [KEY_KP_ADD] = '+', [KEY_KP_SUB] = '-', [KEY_KP_MUL] = '*', [KEY_KP_DIV] = '/',
    [KEY_KP_DECIMAL] = '.', [KEY_KP_ENTER] = '\n'
};

char get_printable_char(key_event_t event) {
    bool shift = (event.status_mask & SHIFT_MASK);
    bool caps  = (event.status_mask & CAPS_MASK);

    bool use_shifted = shift;
    if (event.code >= KEY_A && event.code <= KEY_Z) {
        if (caps) use_shifted = !use_shifted;
    }

    if (use_shifted) {
        return kbd_us_shifted[event.code];
    } else {
        return kbd_us_lower[event.code];
    }
}