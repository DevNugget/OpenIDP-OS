#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <io.h>
#include <io.h>
#include <io.h>
#include <io.h>
#include <io.h>
#include <pic.h>
#include <com1.h>

#define KEY_MOD_SHIFT  0x0100
#define KEY_MOD_CTRL   0x0200
#define KEY_MOD_ALT    0x0400
#define KEY_MOD_SUPER  0x0800

#define KEYBOARD_RING_BUFFER_SIZE 256
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

void keyboard_init(void);
void keyboard_handler(void);
uint16_t keyboard_read_key(void);

#endif