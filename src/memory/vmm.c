#include <vmm.h>

extern uint64_t limine_hhdm;

// Page table index macros
#define PML4_INDEX(x) (((x) >> 39) & 0x1FF)
#define PDPT_INDEX(x) (((x) >> 30) & 0x1FF)
#define PD_INDEX(x)   (((x) >> 21) & 0x1FF)
#define PT_INDEX(x)   (((x) >> 12) & 0x1FF)

#define PAGE_ALIGN_MASK 0x000FFFFFFFFFF000ULL

static inline uint64_t read_cr3(void) {
    uint64_t val;
    asm volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void write_cr3(uint64_t val) {
    asm volatile("mov %0, %%cr3" :: "r"(val) : "memory");
}

static uint64_t* get_next_table(uint64_t* table, size_t index) {
    if (table[index] & VMM_PRESENT) {
        return (uint64_t*)(((table[index] & PAGE_ALIGN_MASK)) + limine_hhdm);
    }

    void* page = pmm_alloc_page();
    if (!page) {
        serial_printf("VMM PANIC: Out of memory allocating page table\n");
        while (1);
    }

    memset(page, 0, PAGE_SIZE);

    uint64_t phys = (uint64_t)page - limine_hhdm;
    table[index] = phys | VMM_PRESENT | VMM_WRITE;

    return (uint64_t*)page;
}

void vmm_map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t* pdpt = get_next_table(pml4, PML4_INDEX(virt));
    uint64_t* pd   = get_next_table(pdpt, PDPT_INDEX(virt));
    uint64_t* pt   = get_next_table(pd,   PD_INDEX(virt));

    pt[PT_INDEX(virt)] = (phys & PAGE_ALIGN_MASK) | flags | VMM_PRESENT;
}

void vmm_unmap_page(uint64_t* pml4, uint64_t virt) {
    uint64_t* pdpt = get_next_table(pml4, PML4_INDEX(virt));
    uint64_t* pd   = get_next_table(pdpt, PDPT_INDEX(virt));
    uint64_t* pt   = get_next_table(pd,   PD_INDEX(virt));

    pt[PT_INDEX(virt)] = 0;
}

uint64_t vmm_get_mapping(uint64_t* pml4, uint64_t virt) {
    uint64_t* pdpt = get_next_table(pml4, PML4_INDEX(virt));
    uint64_t* pd   = get_next_table(pdpt, PDPT_INDEX(virt));
    uint64_t* pt   = get_next_table(pd,   PD_INDEX(virt));

    if (!(pt[PT_INDEX(virt)] & VMM_PRESENT))
        return 0;

    return pt[PT_INDEX(virt)] & PAGE_ALIGN_MASK;
}

uint64_t* vmm_create_pml4(void) {
    uint64_t* pml4 = pmm_alloc_page();
    memset(pml4, 0, PAGE_SIZE);
    return pml4;
}

void vmm_switch_pml4(uint64_t* pml4) {
    uint64_t phys = (uint64_t)pml4 - limine_hhdm;
    write_cr3(phys);
}