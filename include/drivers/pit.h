#ifndef PIT_H
#define PIT_H

#include <stdint.h>
#include <com1.h>

void pit_init(uint32_t frequency);
uint64_t pit_handler(uint64_t current_rsp);

#endif