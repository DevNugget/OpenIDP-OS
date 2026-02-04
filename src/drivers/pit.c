#include <pit.h>
#include <task.h>
#include <idt.h>
#include <io.h>

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQUENCY 1193182  

void pit_init(uint32_t frequency) {
    if (frequency == 0) frequency = 100; 

    uint16_t divisor = PIT_FREQUENCY / frequency;  

    outb(PIT_COMMAND, 0x36);           
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, divisor >> 8);  

    serial_printf("PIT initialized at %d Hz\n", frequency);
}

uint64_t pit_handler(uint64_t current_rsp) {
    // Ask scheduler for next stack
    return scheduler_schedule(current_rsp);
}
