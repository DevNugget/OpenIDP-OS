#include <utility/port.h>

uint8_t inportb(int portnum) {
    uint8_t data = 0;
    __asm__ __volatile__ ("inb %%dx, %%al" : "=a" (data) : "d" (portnum));
    return data;
}

void outportb(int portnum, uint8_t data) {
    __asm__ __volatile__ ("outb %%al, %%dx" :: "a" (data),"d" (portnum));
}
