#include <pit.h>
#include <task.h>
#include <idt.h>
#include <io.h>  // for outb

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQUENCY 1193182  // PIT input clock

// IRQ0 is usually mapped to the PIT
void pit_init(uint32_t frequency) {
    if (frequency == 0) frequency = 100; // default 100Hz

    uint16_t divisor = PIT_FREQUENCY / frequency;  // PIT input clock is 1.193182 MHz

    outb(0x43, 0x36);           // Command byte: channel 0, lobyte/hibyte, mode 3 (square wave)
    outb(0x40, divisor & 0xFF); // Low byte
    outb(0x40, divisor >> 8);   // High byte

    serial_printf("PIT initialized at %d Hz\n", frequency);
}

// IRQ0 handler
uint64_t pit_handler(uint64_t current_rsp) {
    // Instead of just ticking, we ask the scheduler for the next stack
    return scheduler_schedule(current_rsp);
}