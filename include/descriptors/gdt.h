#ifndef GDT_H
#define GDT_H

#include <stdint.h>
#include <com1.h>

// GDT entry flags
#define GDT_PRESENT     0x80    // Bit 7
#define GDT_RING0       0x00    // DPL = 00 (bits 6–5)
#define GDT_RING3       0x60    // DPL = 11 (bits 6–5)

#define GDT_SYSTEM      0x00    // S = 0 (system segment)
#define GDT_CODEDATA    0x10    // S = 1 (code/data segment)

#define GDT_EXECUTABLE  0x08    // E = 1 (code segment)
#define GDT_DATA        0x00    // E = 0 (data segment)

#define GDT_RW          0x02    // Readable code / Writable data
#define GDT_ACCESSED    0x01    // Usually left 0

// GDT entry granularity flags
#define GDT_GRAN_4K     0x80    // G bit (mostly irrelevant in long mode)
#define GDT_SIZE_32     0x40    // D bit (must be 0 for 64-bit code)
#define GDT_LONG_MODE   0x20    // L bit (1 for 64-bit code)
#define GDT_AVL         0x10    // Free for OS use

// GDT entry flag presets
#define GDT_FLAGS_CODE  (GDT_LONG_MODE)
#define GDT_FLAGS_DATA  (0x00)

// GDT entry presets
#define GDT_KERNEL_CODE (GDT_PRESENT | GDT_RING0 | GDT_CODEDATA | GDT_EXECUTABLE | GDT_RW)
#define GDT_KERNEL_DATA (GDT_PRESENT | GDT_RING0 | GDT_CODEDATA | GDT_DATA       | GDT_RW)

#define GDT_USER_CODE   (GDT_PRESENT | GDT_RING3 | GDT_CODEDATA | GDT_EXECUTABLE | GDT_RW)
#define GDT_USER_DATA   (GDT_PRESENT | GDT_RING3 | GDT_CODEDATA | GDT_DATA       | GDT_RW)

// GDT entry indices and selectors
#define GDT_NULL        0
#define GDT_KCODE       1
#define GDT_KDATA       2
#define GDT_UCODE       3
#define GDT_UDATA       4

#define SELECTOR(idx)   ((idx) << 3)

#define KERNEL_CS SELECTOR(GDT_KCODE)  // 0x08
#define KERNEL_DS SELECTOR(GDT_KDATA)  // 0x10
#define USER_CS   SELECTOR(GDT_UCODE)  // 0x18
#define USER_DS   SELECTOR(GDT_UDATA)  // 0x20

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t size;
    uint64_t addr; 
} __attribute__((packed));

extern struct gdt_entry gdt[7];
extern struct gdt_ptr gdtr;

void gdt_init(void);

#endif