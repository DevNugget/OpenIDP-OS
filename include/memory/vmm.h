/* date = February 15th 2026 10:15 pm */

#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include <utility/hhdm.h>

#define BITS_9_MASK   0x1FF
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL
#define CR3_ADDR_MASK 0x000FFFFFFFFFF000ULL

void vmm_init();
void vmm_map_page(phys_addr_t* pml4, virt_addr_t virt, phys_addr_t phys, uint64_t flags);

#define PT_FLAG_PRESENT (1ULL << 0)
#define PT_FLAG_WRITE   (1ULL << 1)
#define PT_FLAG_USER    (1ULL << 2)
#define PT_FLAG_PWT     (1ULL << 3)
#define PT_FLAG_PCD     (1ULL << 4)
#define PT_FLAG_NX      (1ULL << 63)

#define VM_FLAG_NONE   0
#define VM_FLAG_WRITE (1 << 0)
#define VM_FLAG_EXEC  (1 << 1)
#define VM_FLAG_USER  (1 << 2)

typedef struct vm_object {
    uintptr_t base;
    size_t length;
    size_t flags;
    struct vm_object* next;
} vm_object_t;

#endif //VMM_H
