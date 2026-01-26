#include <idt.h>

__attribute__((aligned(0x10))) 
idt_entry_t idt[256]; // Create an array of IDT entries; aligned for performance
idtr_t idtr;

static bool vectors[IDT_MAX_DESCRIPTORS];
extern void* isr_stub_table[];

const char *exception_names[32] = {
    "Divide by Zero",
    "Debug",
    "NMI",
    "Breakpoint",
    "Overflow",
    "Bound Range",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FP Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD FP Exception",
    "Virtualization",
    "Control Protection",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor",
    "VMM",
    "Security",
    "Reserved"
};

void exception_handler(uint64_t vector, uint64_t error) {
    serial_printf("EXCEPTION %d: %s\n", vector, exception_names[vector]);

    if (vector == 14) {
        uint64_t cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
        serial_printf(" PAGE FAULT at %d", cr2);
    }

    serial_printf(" error=%lx\n", error);
    __asm__ volatile ("cli; hlt"); // Completely hangs the computer
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt_entry_t* descriptor = &idt[vector];

    descriptor->isr_low        = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs      = KERNEL_CS;
    descriptor->ist            = 0;
    descriptor->attributes     = flags;
    descriptor->isr_mid        = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high       = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved       = 0;
}

static inline void lidt(idtr_t* idtr) {
    asm volatile("lidt (%0)" :: "r"(idtr) : "memory");
}

static inline void sti() {
    asm volatile("sti");
}

void idt_init() {
    idtr.base = (uintptr_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(idt_entry_t) * IDT_MAX_DESCRIPTORS - 1;

    for (uint8_t vector = 0; vector < 32; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], IDT_INTERRUPT_GATE);
        vectors[vector] = true;
    }

    lidt(&idtr); // load the new IDT
    sti(); // set the interrupt flag
    serial_printf("IDT initialized.\n");
}
