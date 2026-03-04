#include <drivers/pit.h>
#include <utility/port.h>

#define PIT_CMD 0x43
#define PIT_MODE_ONESHOT 0x30

#define PIT_CH0 0x40
#define PIT_FREQ 1193182

void pit_sleep(uint64_t ms) {
    uint16_t count = (uint16_t)((PIT_FREQ * ms) / 1000);

    outportb(PIT_CMD, PIT_MODE_ONESHOT); 
    outportb(PIT_CH0, count & 0xFF);
    outportb(PIT_CH0, count >> 8);

    while (1) {
        outportb(PIT_CMD, 0xE2); 
        uint8_t status = inportb(PIT_CH0);
        if ((status & 0x40) == 0 && (status & 0x80) != 0) {
            break;
        }
    }
}