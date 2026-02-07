#include <ksyscall.h>

extern task_t* current_task;
extern uint64_t limine_hhdm;

/* Helper functions */

static inline uint64_t* phys_to_virt(uint64_t phys) {
    return (uint64_t*)(phys + limine_hhdm);
}

static inline uint64_t virt_to_phys(void* virt) {
    return (uint64_t)virt - limine_hhdm;
}

static inline void invlpg(void* m) {
    asm volatile("invlpg (%0)" :: "r"(m) : "memory");
}

/* Syscall functions */

void* sys_sbrk(intptr_t inc) {
    task_t* task = current_task;
    uint64_t old_break = task->program_break;

    if (inc == 0) {
        return (void*)old_break;
    }

    uint64_t new_break = old_break + inc;
    
    // Calculate page boundaries
    uint64_t old_page_top = (old_break + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint64_t new_page_top = (new_break + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // If we need new pages, allocate and map them
    if (new_page_top > old_page_top) {
        uint64_t num_pages = (new_page_top - old_page_top) / PAGE_SIZE;
        
        // We need the VIRTUAL address of the User PML4 to modify it
        // current_task->cr3 is physical. Convert to HHDM virtual.
        uint64_t* user_pml4 = (uint64_t*)(task->cr3 + limine_hhdm);

        for (uint64_t i = 0; i < num_pages; i++) {
            uint64_t map_addr = old_page_top + (i * PAGE_SIZE);
            void* phys_page = pmm_alloc_page();
            
            if (!phys_page) {
                serial_printf("Out of memory in sys_sbrk!\n");
                return (void*)-1;
            }

            // Map as User | RW | Present (0x7)
            // Note: pmm_alloc_page returns HHDM addr usually, get physical for mapping
            uint64_t phys_addr = (uint64_t)phys_page - limine_hhdm;
            
            vmm_map_page(user_pml4, map_addr, phys_addr, 0x7);
            
            // Zero the memory for security/consistency
            memset((void*)(map_addr), 0, PAGE_SIZE); 
            // Note: We can memset map_addr directly because we are currently 
            // in the context of the user process (CR3 matches), or we are 
            // in kernel space but the user pages are accessible if we are careful.
            // A safer way in a higher-half kernel is to memset via the HHDM address (phys_page).
            memset(phys_page, 0, PAGE_SIZE);
        }
    }

    task->program_break = new_break;
    return (void*)old_break;
}

uint64_t sys_share_mem(int target_pid, size_t size, uint64_t* target_vaddr_out) {
    size = ALIGN_UP(size, PAGE_SIZE);

    task_t* target = get_task_by_pid(target_pid);
    if (!target) return 0;

    // --- FIX START ---
    // Align Current Task's Break to Page Boundary
    if (current_task->program_break & (PAGE_SIZE - 1)) {
        current_task->program_break = (current_task->program_break + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
    uint64_t my_vaddr = current_task->program_break; 
    current_task->program_break += size;

    // Align Target Task's Break to Page Boundary
    if (target->program_break & (PAGE_SIZE - 1)) {
        target->program_break = (target->program_break + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
    uint64_t their_vaddr = target->program_break;
    target->program_break += size;
    // --- FIX END ---

    uint64_t* my_pml4 = (uint64_t*)phys_to_virt(current_task->cr3);
    uint64_t* their_pml4 = (uint64_t*)phys_to_virt(target->cr3);

    for (size_t i = 0; i < size; i += PAGE_SIZE) {
        void* phys = pmm_alloc_page();

        if (!phys) {
            serial_printf("PANIC: PMM returned NULL at offset 0x%x (Target: %d bytes). System Out of Memory!\n", i, size);
            // In a real OS, you would cleanup and return 0. 
            // For debugging now, let's hang so you see the error.
            while(1) asm volatile("hlt");
        }
        uint64_t phys_addr = (uint64_t)phys - limine_hhdm;
        
        // Map in Me
        vmm_map_page(my_pml4, my_vaddr + i, phys_addr, 0x7); // User|RW|Present
        // Map in Them
        vmm_map_page(their_pml4, their_vaddr + i, phys_addr, 0x7);
    }
    
    // Return 'their_vaddr' to the caller so they can send it to the WM
    *target_vaddr_out = their_vaddr;
    return my_vaddr;
}

int sys_unmap(void* vaddr_ptr, size_t size) {
    uint64_t vaddr = (uint64_t)vaddr_ptr;
    
    // 1. Align to page boundaries
    size = ALIGN_UP(size, PAGE_SIZE);
    
    // 2. Get the Virtual Address of the current PML4
    // (cr3 is physical, we need HHDM virtual to read/write it)
    uint64_t* pml4 = (uint64_t*)phys_to_virt(current_task->cr3);

    for (uint64_t i = 0; i < size; i += PAGE_SIZE) {
        uint64_t curr_vaddr = vaddr + i;
        
        // --- Manual Page Table Walk ---
        // We do this manually to ensure we catch missing tables
        
        uint64_t pml4i = PML4_INDEX(curr_vaddr);
        if (!(pml4[pml4i] & 0x1)) continue; // Not present
        
        uint64_t* pdp = (uint64_t*)phys_to_virt(pml4[pml4i] & PAGE_ALIGN_MASK);
        uint64_t pdpi = PDPT_INDEX(curr_vaddr);
        if (!(pdp[pdpi] & 0x1)) continue;

        uint64_t* pd = (uint64_t*)phys_to_virt(pdp[pdpi] & PAGE_ALIGN_MASK);
        uint64_t pdi = PD_INDEX(curr_vaddr);
        if (!(pd[pdi] & 0x1)) continue;

        uint64_t* pt = (uint64_t*)phys_to_virt(pd[pdi] & PAGE_ALIGN_MASK);
        uint64_t pti = PT_INDEX(curr_vaddr);
        
        if (pt[pti] & 0x1) {
            // 3. Found the page!
            uint64_t phys_addr = pt[pti] & PAGE_ALIGN_MASK;
            
            // A. Clear the entry (Unmap)
            pt[pti] = 0;
            
            // B. Invalidate TLB for this address
            invlpg((void*)curr_vaddr);
            
            // C. Free the Physical Memory
            // NOTE: This assumes the other process (the client) is dead or 
            // you don't care if it crashes. Given your current architecture, 
            // this is the correct behavior for cleaning up window buffers.
            //pmm_free_page((void*)phys_to_virt(phys_addr));
        }
    }
    return 0;
}