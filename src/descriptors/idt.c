#include <descriptors/idt.h>
#include <descriptors/gdt.h>
#include <drivers/com1.h>
#include <utility/cpu_state.h>

#define DESCRIPTOR_BYTES 16
#define IDT_SIZE 256

extern char vector_0_handler[];

int_descriptor_t idt[IDT_SIZE];

void load_idt(void* idt_addr) {
    idtr_t idt_reg;
    idt_reg.limit = (DESCRIPTOR_BYTES * IDT_SIZE) - 1;
    idt_reg.base = (uint64_t)idt_addr;
    asm volatile("lidt %0" :: "m"(idt_reg));
}

void set_idt_entry(uint8_t vector, void* handler, uint8_t dpl) {
    uint64_t handler_addr = (uint64_t)handler;
    int_descriptor_t* entry = &idt[vector];
    
    entry->address_low = handler_addr & 0xFFFF;
    entry->address_mid = (handler_addr >> 16) & 0xFFFF;
    entry->address_high = handler_addr >> 32;
    
    entry->selector = KERNEL_CODE;
    
    entry->flags = IDT_INTERRUPT_GATE | IDT_DPL(dpl) | IDT_PRESENT;
    
    entry->ist = 0; // Disable
}

void idt_init() {
    load_idt(idt);
    
    for (int i = 0; i < IDT_SIZE; i++)
        set_idt_entry(i, vector_0_handler + (i * DESCRIPTOR_BYTES), 0);
    
    asm volatile ("sti");
}

cpu_status_t* interrupt_dispatch(cpu_status_t* context) {
    switch (context->vector_number) {
        case 13: {
            serial_write_str("general protection fault.\n");
            break;
        }
        case 14: {
            serial_write_str("page fault.\n");
            for (;;) {
                asm ("hlt");
            }
            break;
        }
        default: {
            serial_write_str("unexpected interrupt.\n");
            break;
        }
    }
    return context;
}