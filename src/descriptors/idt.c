#include <idt.h>

__attribute__((aligned(0x10))) 
idt_entry_t idt[IDT_MAX_DESCRIPTORS];
idtr_t idtr;

static bool vectors[IDT_MAX_DESCRIPTORS];
extern void* isr_stub_table[];

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);
static void setup_isrs();
static void setup_irqs();

static inline void lidt(idtr_t* idtr) {
    asm volatile("lidt (%0)" :: "r"(idtr) : "memory");
}

static inline void sti() {
    asm volatile("sti");
}

void idt_init() {
    idtr.base = (uintptr_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(idt_entry_t) * IDT_MAX_DESCRIPTORS - 1;

    pic_init();

    setup_isrs();
    setup_irqs();

    lidt(&idtr);

    pic_enable_irq(0); // PIT

    sti();
    serial_printf("IDT initialized.\n");
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

uint64_t irq_handler(uint64_t irq, uint64_t current_rsp) {
    uint64_t new_rsp = current_rsp;

    if (irq == IRQ_PIT) { 
        new_rsp = pit_handler(current_rsp);
    } else if (irq == IRQ_KEYBOARD) {
        keyboard_handler();
    }
    
    // Acknowledge PIC
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);

    return new_rsp; // Goes into RAX and idt.asm puts it in RSP
}

static void setup_isrs() {
    for (uint8_t vector = 0; vector < ISR_COUNT; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], IDT_INTERRUPT_GATE);
        vectors[vector] = true;
    }
}

static void setup_irqs() {
    for (uint8_t i = 0; i < IRQ_COUNT; i++) {
        idt_set_descriptor(ISR_COUNT + i, isr_stub_table[ISR_COUNT + i], IDT_INTERRUPT_GATE);
        vectors[ISR_COUNT + i] = true;
    }

    idt_set_descriptor(0x80, syscall_stub, IDT_USER_INTERRUPT);
}
