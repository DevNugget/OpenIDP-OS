/* date = February 12th 2026 6:05 pm */

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define IDT_INTERRUPT_GATE 0b1110
#define IDT_TRAP_GATE      0b1111
#define IDT_PRESENT        (1 << 7)
#define IDT_DPL(X)         ((X & 0b11) << 5)

typedef struct int_descriptor {
    uint16_t address_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t address_mid;
    uint32_t address_high;
    uint32_t reserved;
} __attribute__((packed)) int_descriptor_t;

typedef struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

void idt_init();

#endif //IDT_H
