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

void exception_handler(uint64_t vector, uint64_t error, uint64_t rip) {
    serial_printf("EXCEPTION %d: %s\n", vector, exception_names[vector]);
    serial_printf("RIP: 0x%x\n", (void*)rip);

    if (vector == 14) {
        pagefault_handler(error, rip);
    }

    if (vector == 13) { // General Protection Fault
        serial_printf("General Protection Fault details:\n");
        
        // Decode error code
        serial_printf("  External: %s\n", (error & 1) ? "yes" : "no");
        serial_printf("  Table: %s\n", 
            (error & (1 << 1)) ? "IDT" : 
            (error & (1 << 2)) ? "LDT" : "GDT");
        serial_printf("  Index: %d\n", (error >> 3) & 0x1FFF);
        
        // Print segment registers
        uint64_t cs, ds, ss;
        __asm__ volatile (
            "mov %%cs, %0\n\t"
            "mov %%ds, %1\n\t"
            "mov %%ss, %2\n\t"
            : "=r"(cs), "=r"(ds), "=r"(ss)
        );
        serial_printf("Segment registers:\n");
        serial_printf("  CS: 0x%x\n", (void*)cs);
        serial_printf("  DS: 0x%x\n", (void*)ds);
        serial_printf("  SS: 0x%x\n", (void*)ss);
    }
    
    // Print stack trace (simple version)
    serial_printf("Stack trace (RBP chain):\n");
    uint64_t* rbp;
    __asm__ volatile ("mov %%rbp, %0" : "=r"(rbp));
    
    for (int i = 0; i < 10 && rbp && (uint64_t)rbp >= 0xFFFF800000000000; i++) {
        uint64_t return_addr = *(rbp + 1);
        serial_printf("  [%d] RBP=%x -> RIP=%x\n", i, rbp, (void*)return_addr);
        rbp = (uint64_t*)*rbp;
    }

    serial_printf(" error=%x\n", error);
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

    pic_init();

    for (uint8_t vector = 0; vector < 32; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], IDT_INTERRUPT_GATE);
        vectors[vector] = true;
    }

    // Set up IRQ handlers (32-47)
    for (uint8_t i = 0; i < 16; i++) {
        idt_set_descriptor(32 + i, isr_stub_table[32 + i], IDT_INTERRUPT_GATE);
        vectors[32 + i] = true;
    }

    lidt(&idtr); // load the new IDT

    pic_enable_irq(0); // Enable IRQ0 (PIT)

    sti(); // set the interrupt flag
    serial_printf("IDT initialized.\n");
}

uint64_t irq_handler(uint64_t irq, uint64_t current_rsp) {
    uint64_t new_rsp = current_rsp;

    if (irq == 32) { // PIT (Timer)
        // pit_handler now needs to return the new stack pointer
        new_rsp = pit_handler(current_rsp);
    }
    
    // Acknowledge PIC
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);

    return new_rsp; // This value goes into RAX, and idt.asm puts it in RSP
}
