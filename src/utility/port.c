#include <utility/port.h>

uint8_t inportb(int portnum) {
    uint8_t data = 0;
    __asm__ __volatile__ ("inb %%dx, %%al" : "=a" (data) : "d" (portnum));
    return data;
}

void outportb(int portnum, uint8_t data) {
    __asm__ __volatile__ ("outb %%al, %%dx" :: "a" (data),"d" (portnum));
}

uint32_t inportl(int portnum) {
    uint32_t data = 0;
    __asm__ __volatile__ ("inl %%dx, %%eax" : "=a" (data) : "d" (portnum));
    return data;
}

void outportl(int portnum, uint32_t data) {
    __asm__ __volatile__ ("outl %%eax, %%dx" :: "a" (data),"d" (portnum));
}