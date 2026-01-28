#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include <pmm.h>
#include <com1.h>
#include <kstring.h>

#define PAGE_SIZE 4096

// Page table flags
#define VMM_PRESENT   (1ULL << 0)
#define VMM_WRITE     (1ULL << 1)
#define VMM_USER      (1ULL << 2)
#define VMM_PWT       (1ULL << 3)
#define VMM_PCD       (1ULL << 4)
#define VMM_ACCESSED  (1ULL << 5)
#define VMM_DIRTY     (1ULL << 6)
#define VMM_HUGE      (1ULL << 7)
#define VMM_GLOBAL    (1ULL << 8)
#define VMM_NOEXEC    (1ULL << 63)

// Page table index macros
#define PML4_INDEX(x) (((x) >> 39) & 0x1FF)
#define PDPT_INDEX(x) (((x) >> 30) & 0x1FF)
#define PD_INDEX(x)   (((x) >> 21) & 0x1FF)
#define PT_INDEX(x)   (((x) >> 12) & 0x1FF)

#define PAGE_ALIGN_MASK 0x000FFFFFFFFFF000ULL

uint64_t read_cr3(void);
void write_cr3(uint64_t val);
uint64_t* vmm_create_pml4(void);

void vmm_map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap_page(uint64_t* pml4, uint64_t virt);

uint64_t vmm_get_mapping(uint64_t* pml4, uint64_t virt);
void vmm_switch_pml4(uint64_t* pml4);
uint64_t* vmm_create_process_pml4(uint64_t* master_kernel_pml4);

#endif