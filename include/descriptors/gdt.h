/* date = February 12th 2026 4:22 pm */

#ifndef GDT_H
#define GDT_H

#include <stdint.h>
#include <stddef.h>

#define NULL_SELECTOR 0x00
#define KERNEL_CODE   0x08
#define KERNEL_DATA   0x10
#define USER_CODE     0x18
#define USER_DATA     0x20
#define TSS_SELECTOR  0x28

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

typedef struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss64_t;

void gdt_init(void);
void gdt_init_cpu(size_t cpu_index);
void gdt_set_tss_rsp0(size_t cpu_index, uint64_t rsp0);

#endif //GDT_H
