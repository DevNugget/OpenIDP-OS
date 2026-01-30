#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <io.h>
#include <pic.h>
#include <com1.h>

#define KEYBOARD_RING_BUFFER_SIZE 256
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

void keyboard_init(void);
void keyboard_handler(void);
char keyboard_read_char(void);

#endif