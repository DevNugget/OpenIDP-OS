#ifndef IDT_H
#define IDT_H 1

#include <stdint.h>
#include <stdbool.h>
#include <gdt.h>
#include <com1.h>
#include <pagefault.h>

#define IDT_MAX_DESCRIPTORS 256

#define IDT_GATE_PRESENT   (1 << 7)
#define IDT_GATE_RING0     (0 << 5)
#define IDT_GATE_RING3     (3 << 5)
#define IDT_GATE_INTERRUPT 0xE
#define IDT_GATE_TRAP      0xF

#define IDT_INTERRUPT_GATE (IDT_GATE_PRESENT | IDT_GATE_RING0 | IDT_GATE_INTERRUPT)
#define IDT_TRAP_GATE      (IDT_GATE_PRESENT | IDT_GATE_RING0 | IDT_GATE_TRAP)
#define IDT_USER_INTERRUPT (IDT_GATE_PRESENT | IDT_GATE_RING3 | IDT_GATE_INTERRUPT)

typedef struct {
	uint16_t    isr_low;      // The lower 16 bits of the ISR's address
	uint16_t    kernel_cs;    // The GDT segment selector that the CPU will load into CS before calling the ISR
	uint8_t	    ist;          // The IST in the TSS that the CPU will load into RSP; set to zero for now
	uint8_t     attributes;   // Type and attributes; see the IDT page
	uint16_t    isr_mid;      // The higher 16 bits of the lower 32 bits of the ISR's address
	uint32_t    isr_high;     // The higher 32 bits of the ISR's address
	uint32_t    reserved;     // Set to zero
} __attribute__((packed)) idt_entry_t;

typedef struct {
	uint16_t	limit;
	uint64_t	base;
} __attribute__((packed)) idtr_t;

__attribute__((aligned(0x10))) 
extern idt_entry_t idt[IDT_MAX_DESCRIPTORS]; // Create an array of IDT entries; aligned for performance
extern idtr_t idtr;

void exception_handler(uint64_t vector, uint64_t error, uint64_t rip);

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);
void idt_init(void);

#endif