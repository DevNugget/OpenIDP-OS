#include <gdt.h>

struct gdt_entry gdt[7];
struct gdt_ptr gdtr;

extern tss_t tss;

static void gdt_set_entry(int i, uint8_t access, uint8_t flags) {
    gdt[i] = (struct gdt_entry){
        .limit_low = 0,
        .base_low = 0,
        .base_middle = 0,
        .access = access,
        .granularity = flags,
        .base_high = 0
    };
}

static inline void lgdt(struct gdt_ptr* gdtr) {
    asm volatile("lgdt (%0)" :: "r"(gdtr) : "memory");
}

static inline void gdt_flush(void) {
    asm volatile (
        "mov %[data], %%ax \n"
        "mov %%ax, %%ds \n"
        "mov %%ax, %%es \n"
        "mov %%ax, %%fs \n"
        "mov %%ax, %%gs \n"
        "mov %%ax, %%ss \n"

        "pushq %[code]      \n"
        "lea 1f(%%rip), %%rax \n"
        "push %%rax       \n"
        "retfq            \n"
        "1:               \n"
        :
        : [code] "i"(KERNEL_CS), 
           [data] "i"(KERNEL_DS)
        : "rax", "memory"
    );
}

void gdt_init(void) {
    gdt_set_entry(0, 0x00, 0x00); // null

    gdt_set_entry(1, GDT_KERNEL_CODE, GDT_FLAGS_CODE);
    gdt_set_entry(2, GDT_KERNEL_DATA, GDT_FLAGS_DATA); 

    gdt_set_entry(3, GDT_USER_CODE, GDT_FLAGS_CODE);
    gdt_set_entry(4, GDT_USER_DATA, GDT_FLAGS_DATA);

    uint64_t tss_base = (uint64_t)&tss;
    uint64_t tss_limit = sizeof(tss) - 1;

    gdt[5].limit_low = tss_limit & 0xFFFF;
    gdt[5].base_low = tss_base & 0xFFFF;
    gdt[5].base_middle = (tss_base >> 16) & 0xFF;
    gdt[5].access = 0x89; // Present, executable, accessed
    gdt[5].granularity = 0; 
    gdt[5].base_high = (tss_base >> 24) & 0xFF;
    
    // Entry 6 is the upper 32 bits of the TSS base
    struct gdt_entry* high_part = (struct gdt_entry*)&gdt[6];
    *(uint64_t*)high_part = (tss_base >> 32);

    gdtr.size = sizeof(gdt) - 1;
    gdtr.addr = (uint64_t)&gdt;

    lgdt(&gdtr);
    gdt_flush();

    asm volatile("ltr %0" :: "r"((uint16_t)0x28));

    serial_printf("GDT initialized.\n");
}