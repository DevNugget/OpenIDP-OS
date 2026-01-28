#ifndef PAGEFAULT_H
#define PAGEFAULT_H

#include <stdint.h>
#include <com1.h>

void pagefault_handler(uint64_t error_code, uint64_t rip);

#endif 