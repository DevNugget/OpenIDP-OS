#include <pagefault.h>

static inline uint64_t read_cr2(void) {
    uint64_t val;
    asm volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

void pagefault_handler(uint64_t error_code, uint64_t rip) {
    uint64_t fault_addr = read_cr2();

    serial_printf("\n=== PAGE FAULT ===\n");
    serial_printf("Fault address: 0x%x\n", fault_addr);
    serial_printf("RIP:           0x%x\n", rip);
    serial_printf("Error code:    0x%x\n", error_code);

    serial_printf("Reason:\n");

    if (!(error_code & 1))
        serial_printf(" - Page not present\n");

        //void* phys = pmm_alloc_page();
        //if (!phys) {
        //    serial_printf("PAGEFAULT HANDLER: Out of physical memory\n");
        //}

    else
        serial_printf(" - Protection violation\n");

    if (error_code & (1 << 1))
        serial_printf(" - Write access\n");
    else
        serial_printf(" - Read access\n");

    if (error_code & (1 << 2))
        serial_printf(" - From user mode\n");
    else
        serial_printf(" - From kernel mode\n");

    if (error_code & (1 << 3))
        serial_printf(" - Reserved bits overwritten\n");

    if (error_code & (1 << 4))
        serial_printf(" - Instruction fetch (NX)\n");

    serial_printf("SYSTEM HALTED\n");

    for (;;) {
        asm volatile("hlt");
    }
}