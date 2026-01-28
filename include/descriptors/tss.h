#ifndef TSS_H
#define TSS_H

#include <stdint.h>
#include <gdt.h>
#include <kstring.h>

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;      // Most important field
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

void tss_init(void);
void tss_set_rsp0(uint64_t rsp0);

#endif