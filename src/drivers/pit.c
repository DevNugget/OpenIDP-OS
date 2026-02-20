#include <drivers/pit.h>
#include <utility/port.h>

#define PIT_CMD 0x43
#define PIT_MODE_ONESHOT 0x30

#define PIT_CH0 0x40
#define PIT_FREQ 1193182

void pit_sleep(uint64_t ms) {
    uint16_t count = (uint16_t)((PIT_FREQ * ms)/1000);

    outportb(PIT_CMD, PIT_MODE_ONESHOT);
    outportb(PIT_CH0, count & 0xFF);
    outportb(PIT_CH0, count >> 8);

    uint16_t last = 0xFFFF;
    while (1) {
        outportb(PIT_CMD, 0x00); 
        
        uint8_t low = inportb(PIT_CH0);
        uint8_t high = inportb(PIT_CH0);
        uint16_t current_count = (high << 8) | low;
        
        if (current_count > last) break;
        last = current_count;
    }
}