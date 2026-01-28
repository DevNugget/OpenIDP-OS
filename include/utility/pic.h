#ifndef PIC_H
#define PIC_H

#include <stdint.h>
#include <io.h>
#include <com1.h>

void pic_init();
void pic_enable_irq(uint8_t irq);

#endif