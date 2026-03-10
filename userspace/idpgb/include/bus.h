#ifndef BUS_H
#define BUS_H

#include <stdint.h>

uint8_t bus_read(uint16_t addr);
void bus_write(uint16_t addr, uint8_t value);

uint16_t bus_read16(uint16_t address);
void bus_write16(uint16_t address, uint16_t value);

#endif