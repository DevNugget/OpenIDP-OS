/* date = February 12th 2026 0:42 pm */

#ifndef COM1_H
#define COM1_H

#include <stdint.h>

int serial_init();
void serial_write_str(char* str);
void serial_u64_dec(uint64_t n);
void serial_u64_hex(uint64_t n);
void serial_printf(const char* fmt_str, ...);

#endif //COM1_H
