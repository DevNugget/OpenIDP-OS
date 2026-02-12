/* date = February 12th 2026 4:22 pm */

#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#define NULL_SELECTOR 0x00
#define KERNEL_CODE   0x08
#define KERNEL_DATA   0x10
#define USER_CODE     0x18
#define USER_DATA     0x20

#define GDT_TYPE_CODE   0b1011
#define GDT_TYPE_DATA   0b0011   
#define GDT_S_CODE_DATA (1 << 12)
#define GDT_DPL0        (0 << 13)
#define GDT_DPL3        (3 << 13)
#define GDT_PRESENT     (1 << 15)
#define GDT_LONG_MODE   (1 << 21)

typedef struct gdtr {
    uint16_t limit;
    uint64_t address;
}__attribute__((packed)) gdtr_t ;

void gdt_init();

#endif //GDT_H
