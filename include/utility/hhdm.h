/* date = February 13th 2026 0:01 pm */

#ifndef HHDM_H
#define HHDM_H

#include <stdint.h>

typedef uint64_t phys_addr_t;
typedef uint64_t virt_addr_t;

uint64_t get_hhdm();
void hhdm_request_offset();
void* phys_to_virt(uint64_t phys);
phys_addr_t virt_to_phys(virt_addr_t virt);

#endif //HHDM_H
