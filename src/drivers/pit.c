#include <drivers/pit.h>
#include <utility/port.h>

#define PIT_CMD 0x43
#define PIT_MODE_ONESHOT 0x30

#define PIT_CH0 0x40
#define PIT_FREQ 1193182

void pit_sleep(uint64_t ms) {
    uint16_t count = (uint16_t)((PIT_FREQ * ms) / 1000);

    outportb(PIT_CMD, PIT_MODE_ONESHOT); // 0x30
    outportb(PIT_CH0, count & 0xFF);
    outportb(PIT_CH0, count >> 8);

    uint16_t prev_tick = count;
    while (1) {
        // Command 0x00: Latch counter for channel 0
        outportb(PIT_CMD, 0x00); 
        uint8_t lo = inportb(PIT_CH0);
        uint8_t hi = inportb(PIT_CH0);
        uint16_t current_tick = ((uint16_t)hi << 8) | lo;

        // In Mode 0, the timer counts down. If it wraps to 0xFFFF or hits 0, it's done.
        if (current_tick == 0 || current_tick > prev_tick) {
            break;
        }
        prev_tick = current_tick;
    }
}