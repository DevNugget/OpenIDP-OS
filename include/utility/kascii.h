/* date = February 17th 2026 5:42 pm */

#ifndef KASCII_H
#define KASCII_H

#include <stdint.h>
#include <stdbool.h>

#include <drivers/keyboard.h>

char get_printable_char(key_event_t event);

#endif //KASCII_H
