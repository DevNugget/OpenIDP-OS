/* date = February 13th 2026 0:01 pm */

#ifndef HHDM_H
#define HHDM_H

#include <stdint.h>

uint64_t get_hhdm();
void hhdm_request_offset();
void* phys_to_virt(uint64_t phys);

#endif //HHDM_H
