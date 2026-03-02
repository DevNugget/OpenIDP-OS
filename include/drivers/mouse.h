#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct mouse_event {
    int16_t delta_x;
    int16_t delta_y;
    uint8_t buttons;
} mouse_event_t;

void mouse_init(void);
void mouse_driver_irq_handler(void);
bool mouse_poll(void);
mouse_event_t mouse_read(void);

#endif
