#include <vmm.h>

extern uint64_t limine_hhdm;

/* Helper functions */

uint64_t read_cr3(void) {
    uint64_t val;
    asm volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

void write_cr3(uint64_t val) {
    asm volatile("mov %0, %%cr3" : : "r"(val) : "memory");
}

static inline void invlpg(uint64_t virt) {
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

static inline uint64_t* phys_to_virt(uint64_t phys) {
    return (uint64_t*)(phys + limine_hhdm);
}

static inline uint64_t virt_to_phys(void* virt) {
    return (uint64_t)virt - limine_hhdm;
}

static uint64_t* get_or_alloc_table(uint64_t* table, size_t index) {
    if (table[index] & VMM_PRESENT) {
        if (!(table[index] & VMM_USER)) {
             table[index] |= VMM_USER;
        }

        uint64_t phys_addr = table[index] & PAGE_ALIGN_MASK;
        return phys_to_virt(phys_addr);
    }
    
    void* new_table = pmm_alloc_page();
    if (!new_table) {
        serial_printf("VMM PANIC: Out of memory allocating page table\n");
        while (1);
    }
    
    memset(new_table, 0, PAGE_SIZE);
    
    uint64_t new_phys = virt_to_phys(new_table);
    
    table[index] = new_phys | VMM_PRESENT | VMM_WRITE | VMM_USER;
    
    return (uint64_t*)new_table;
}

static uint64_t* get_table_noalloc(uint64_t* table, size_t index) {
    if (table[index] & VMM_PRESENT) {
        uint64_t phys_addr = table[index] & PAGE_ALIGN_MASK;
        return phys_to_virt(phys_addr);
    }
    return NULL;
}

/* Allocation functions */

void vmm_map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t* pdpt = get_or_alloc_table(pml4, PML4_INDEX(virt));
    uint64_t* pd   = get_or_alloc_table(pdpt, PDPT_INDEX(virt));
    uint64_t* pt   = get_or_alloc_table(pd, PD_INDEX(virt));
    
    // Map the page
    uint64_t entry = (phys & PAGE_ALIGN_MASK) | flags | VMM_PRESENT;
    pt[PT_INDEX(virt)] = entry;
    
    // Invalidate TLB entry
    invlpg(virt);
}

void vmm_unmap_page(uint64_t* pml4, uint64_t virt) {
    uint64_t* pdpt = get_table_noalloc(pml4, PML4_INDEX(virt));
    if (!pdpt) return;
    
    uint64_t* pd = get_table_noalloc(pdpt, PDPT_INDEX(virt));
    if (!pd) return;
    
    uint64_t* pt = get_table_noalloc(pd, PD_INDEX(virt));
    if (!pt) return;
    
    // Clear the entry if present
    size_t pt_index = PT_INDEX(virt);
    if (pt[pt_index] & VMM_PRESENT) {
        pt[pt_index] = 0;
        invlpg(virt);
    }
}

uint64_t vmm_get_mapping(uint64_t* pml4, uint64_t virt) {
    uint64_t* pdpt = get_table_noalloc(pml4, PML4_INDEX(virt));
    if (!pdpt) return 0;
    
    uint64_t* pd = get_table_noalloc(pdpt, PDPT_INDEX(virt));
    if (!pd) return 0;
    
    uint64_t* pt = get_table_noalloc(pd, PD_INDEX(virt));
    if (!pt) return 0;
    
    uint64_t entry = pt[PT_INDEX(virt)];
    if (!(entry & VMM_PRESENT)) return 0;
    
    return entry & PAGE_ALIGN_MASK;
}

uint64_t* vmm_create_pml4(void) {
    uint64_t* pml4 = pmm_alloc_page();
    if (!pml4) return NULL;
    
    memset(pml4, 0, PAGE_SIZE);
    
    return pml4;
}

void vmm_switch_pml4(uint64_t* pml4) {
    uint64_t phys = virt_to_phys(pml4);
    write_cr3(phys);
}

uint64_t* vmm_create_process_pml4(uint64_t* master_kernel_pml4) {
    // 1. Allocate a physical page for the new PML4
    void* new_pml4_phys = pmm_alloc_page();
    if (!new_pml4_phys) return NULL;

    // 2. Get the virtual address so we can write to it
    // Note: pmm_alloc_page usually returns HHDM address in many setups, 
    // adjust if your pmm returns pure physical addresses.
    uint64_t* new_pml4_virt = (uint64_t*)new_pml4_phys; 

    // 3. Zero out the lower half (User Space)
    for (int i = 0; i < 256; i++) {
        new_pml4_virt[i] = 0;
    }

    // 4. Copy the upper half (Kernel Space) from the master PML4
    // This includes the HHDM, Kernel binary, and Kernel Heap
    for (int i = 256; i < 512; i++) {
        new_pml4_virt[i] = master_kernel_pml4[i];
    }

    // Return the physical address (which is what CR3 expects)
    return (uint64_t*)virt_to_phys(new_pml4_virt);
}
