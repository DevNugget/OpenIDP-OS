#include <pic.h>

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

void pic_init() {
    // ICW1: start initialization
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);
    
    // ICW2: set interrupt vector offsets
    outb(PIC1_DATA, 0x20); // IRQ 0-7 mapped to 0x20-0x27
    outb(PIC2_DATA, 0x28); // IRQ 8-15 mapped to 0x28-0x2F
    
    // ICW3: master/slave wiring
    outb(PIC1_DATA, 0x04); // IRQ2 connects to slave
    outb(PIC2_DATA, 0x02); // Slave ID is 2
    
    // ICW4: enable 80x86 mode
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    
    // Mask all interrupts initially
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    
    serial_printf("PIC initialized\n");
}

void pic_enable_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}
