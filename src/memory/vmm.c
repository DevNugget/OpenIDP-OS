#include <memory/vmm.h>
#include <memory/pmm.h>
#include <drivers/com1.h>
#include <utility/kstring.h>

virt_addr_t* kernel_pml4 = NULL;

static inline phys_addr_t read_cr3() {
    phys_addr_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r" (cr3));
    return cr3;
}

static inline void write_cr3(phys_addr_t val) {
    __asm__ volatile("mov %0, %%cr3" :: "r" (val) : "memory");
}

static inline phys_addr_t cr3_phys_addr(uint64_t pte) {
    return (pte & CR3_ADDR_MASK);
}

void vmm_init() {
    phys_addr_t pml4_phys = pmm_alloc(1);
    kernel_pml4 = (virt_addr_t*)phys_to_virt(pml4_phys);
    memset(kernel_pml4, 0, PAGE_SIZE);

    phys_addr_t current_cr3 = read_cr3();
    virt_addr_t* current_pml4 = (virt_addr_t*)phys_to_virt(cr3_phys_addr(current_cr3));

    for (uint64_t i = 256; i < 512; i++) {
        kernel_pml4[i] = current_pml4[i];
    }

    uint64_t new_cr3 =
    (pml4_phys & CR3_ADDR_MASK) |
    (current_cr3 & ~CR3_ADDR_MASK);

    write_cr3(new_cr3);
    serial_printf("[VMM](vmm_init) Initialized. Switched to new PML4 @(PHYS)0x%x\n", pml4_phys);
}

static inline size_t pml4_idx(virt_addr_t addr) {
    return ((addr >> 39) & BITS_9_MASK);
}

static inline size_t pdpr_idx(virt_addr_t addr) {
    return ((addr >> 30) & BITS_9_MASK);
}

static inline size_t pd_idx(virt_addr_t addr) {
    return ((addr >> 21) & BITS_9_MASK);
}

static inline size_t pt_idx(virt_addr_t addr) {
    return ((addr >> 12) & BITS_9_MASK);
}

static inline phys_addr_t pte_phys_addr(uint64_t pte) {
    return (pte & PTE_ADDR_MASK);
}

void vmm_map_page(phys_addr_t* pml4, virt_addr_t virt, phys_addr_t phys, uint64_t flags) {
    size_t pml4_index = pml4_idx(virt);
    size_t pdpr_index = pdpr_idx(virt);
    size_t pd_index = pd_idx(virt);
    size_t pt_index = pt_idx(virt);

    // pml4 to pdpr
    if (!(pml4[pml4_index] & PT_FLAG_PRESENT)) {
        phys_addr_t new_pdpr_phys = pmm_alloc(1);
        virt_addr_t* new_pdpr_virt = (virt_addr_t*)phys_to_virt(new_pdpr_phys);
        memset(new_pdpr_virt, 0, PAGE_SIZE);

        pml4[pml4_index] = new_pdpr_phys | PT_FLAG_PRESENT | PT_FLAG_WRITE | PT_FLAG_USER;
    }
    virt_addr_t* pdpr = (virt_addr_t*)phys_to_virt(pte_phys_addr(pml4[pml4_index]));

    // pdpr to pd
    if (!(pdpr[pdpr_index] & PT_FLAG_PRESENT)) {
        phys_addr_t new_pd_phys = pmm_alloc(1);
        virt_addr_t* new_pd_virt = (virt_addr_t*)phys_to_virt(new_pd_phys);
        memset(new_pd_virt, 0, PAGE_SIZE);

        pdpr[pdpr_index] = new_pd_phys | PT_FLAG_PRESENT | PT_FLAG_WRITE | PT_FLAG_USER;
    }
    virt_addr_t* pd = (virt_addr_t*)phys_to_virt(pte_phys_addr(pdpr[pdpr_index]));

    // pd to pt
    if (!(pd[pd_index] & PT_FLAG_PRESENT)) {
        phys_addr_t new_pt_phys = pmm_alloc(1);
        virt_addr_t* new_pt_virt = (virt_addr_t*)phys_to_virt(new_pt_phys);
        memset(new_pt_virt, 0, PAGE_SIZE);

        pd[pd_index] = new_pt_phys | PT_FLAG_PRESENT | PT_FLAG_WRITE | PT_FLAG_USER;
    }
    virt_addr_t* pt = (virt_addr_t*)phys_to_virt(pte_phys_addr(pd[pd_index]));

    pt[pt_index] = phys | flags | PT_FLAG_PRESENT;

    __asm__ volatile("invlpg (%0)" :: "r" (virt) : "memory");
}

uint64_t convert_x86_64_vm_flags(size_t flags) {
    uint64_t value = PT_FLAG_PRESENT;
    if (flags & VM_FLAG_WRITE)
        value |= PT_FLAG_WRITE;
    if (flags & VM_FLAG_USER)
        value |= PT_FLAG_USER;
    if ((flags & VM_FLAG_EXEC) == 0)
        value |= PT_FLAG_NX;
    return value;
};