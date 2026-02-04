#include <exception.h>

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
    
    // Print stack trace
    serial_printf("Stack trace (RBP chain):\n");
    uint64_t* rbp;
    __asm__ volatile ("mov %%rbp, %0" : "=r"(rbp));
    
    for (int i = 0; i < 10 && rbp && (uint64_t)rbp >= 0xFFFF800000000000; i++) {
        uint64_t return_addr = *(rbp + 1);
        serial_printf("  [%d] RBP=%x -> RIP=%x\n", i, rbp, (void*)return_addr);
        rbp = (uint64_t*)*rbp;
    }

    serial_printf(" error=%x\n", error);
    __asm__ volatile ("cli; hlt");
}
