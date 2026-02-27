/* date = February 12th 2026 0:37 pm */

#ifndef PORT_H
#define PORT_H

#include <stdint.h>

uint8_t inportb(int portnum);
void outportb(int portnum, uint8_t data);

uint32_t inportl(int portnum);
void outportl(int portnum, uint32_t data);

#endif //PORT_H
