/* date = February 13th 2026 10:02 pm */

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <utility/hhdm.h>

#define PAGE_SIZE 4096
#define BITS_PER_ROW 64

void pmm_init();
phys_addr_t pmm_alloc(size_t frame_count);
void pmm_free(phys_addr_t addr, size_t frame_count);

uint64_t pmm_get_total_pages(void);
uint64_t pmm_get_used_pages(void);

#endif //PMM_H
