#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <stdint.h>
#include <com1.h>
#include <pagefault.h>

#define ERR_VECTOR_GPF 13
#define ERR_VECTOR_PAGEFAULT 14

void exception_handler(uint64_t vector, uint64_t error, uint64_t rip);

#endif


