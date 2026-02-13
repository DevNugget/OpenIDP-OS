/* date = February 13th 2026 4:00 pm */

#ifndef APIC_H
#define APIC_H

#include <stdint.h>

/* APIC */
#define IA32_APIC_BASE 0x1B

// Local APIC register offsets
#define SPURIOUS_OFFSET 0xF0
#define EOI_OFFSET      0xB0
#define TIMERLVT_OFFSET 0x320
#define LAPIC_ID_OFFSET 0x20

#define SPURIOUS_VECTOR 0xFF

typedef union {
    uint64_t raw;
    struct {
        uint64_t reserved0 : 8;   
        uint64_t bsp       : 1;   
        uint64_t reserved1 : 2;   
        uint64_t apic_en   : 1;   
        uint64_t apic_base : 20;  
        uint64_t reserved2 : 32;
    } __attribute__((packed));
} ia32_apic_base_t;

void apic_init();

/* PIC 8259 ports & ICW */
#define PIC_COMMAND_MASTER 0x20
#define PIC_DATA_MASTER    0x21
#define PIC_COMMAND_SLAVE  0xA0
#define PIC_DATA_SLAVE     0xA1
#define ICW_1   0x11
#define ICW_2_M 0x20
#define ICW_2_S 0x28
#define ICW_3_M 0x04
#define ICW_3_S 0x02
#define ICW_4   0x01

#endif //APIC_H
