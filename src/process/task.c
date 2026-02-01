#include <task.h>

extern uint64_t* kernel_pml4; // From main.c
extern uint64_t limine_hhdm;
extern void exit_switch_to(uint64_t rsp);

extern struct limine_framebuffer* framebuffer;

task_t* current_task = NULL;
task_t* task_head = NULL;
static uint64_t next_pid = 1;

static task_t* zombie_task = NULL;

void init_scheduler(void) {
    // Create a task wrapper for the currently running kernel code
    // so we can switch *back* to it later.
    task_t* root_task = (task_t*)kmalloc(sizeof(task_t));
    root_task->pid = 0;
    root_task->next = root_task; // Circular list
    root_task->rsp = 0; // Will be set by the first interrupt

    current_task = root_task;
    task_head = root_task;
    
    serial_printf("Scheduler initialized. Root task PID 0 created.\n");
}

void create_kernel_task(void (*entry_point)()) {
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    
    // Allocate a stack for the new task (4KB)
    uint64_t stack_bottom = (uint64_t)kmalloc(4096);
    uint64_t stack_top = stack_bottom + 4096;

    // We need to put a "fake" interrupt frame on this stack
    // so when the scheduler switches to it, iretq has something to pop.
    
    // Make room for the registers context
    uint64_t* sp = (uint64_t*)stack_top;
    
    // 1. Hardware Pushes (SS, RSP, RFLAGS, CS, RIP)
    // We strictly assume x86_64 kernel privileges for now.
    sp--; *sp = KERNEL_DS;          // SS (Kernel Data Segment) - usually 0x10 in Limine/GDT
    sp--; *sp = stack_top;     // RSP
    sp--; *sp = 0x202;         // RFLAGS (Interrupts Enabled | Reserved bit)
    sp--; *sp = KERNEL_CS;          // CS (Kernel Code Segment) - usually 0x08
    sp--; *sp = (uint64_t)entry_point; // RIP

    // 2. General Purpose Registers (R15...RAX)
    // Push 0 for all of them
    for (int i = 0; i < 15; i++) {
        sp--; *sp = 0;
    }

    // Update the task structure
    new_task->rsp = (uint64_t)sp;
    new_task->pid = next_pid++;
    new_task->cr3 = (uint64_t)vmm_create_process_pml4(kernel_pml4);
    
    // Add to linked list (Round Robin)
    new_task->next = task_head->next;
    task_head->next = new_task;
    
    serial_printf("Task created: PID %d\n", new_task->pid);
}

// Helper to switch CR3, copy data, and switch back
void copy_to_user_mem(uint64_t* user_pml4, uint64_t vaddr, void* data, uint64_t size) {
    uint64_t old_cr3 = read_cr3();
    write_cr3((uint64_t)user_pml4);
    memcpy((void*)vaddr, data, size);
    write_cr3(old_cr3);
}

static inline uint64_t get_phys_addr(void* addr) {
    return (uint64_t)addr - limine_hhdm;
}

void create_user_process(void* elf_data) {
    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_data;

    // 1. Verify Magic
    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' || 
        header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        serial_printf("Invalid ELF Magic\n");
        return;
    }

    // 2. Create Process Address Space
    // We need the VIRTUAL address to write to it, and PHYSICAL to give to CR3 later
    uint64_t* pml4_virt = pmm_alloc_page(); 
    uint64_t* pml4_phys = (uint64_t*)get_phys_addr(pml4_virt);
    
    // Initialize PML4: clear user half, copy kernel half
    memset(pml4_virt, 0, 256 * sizeof(uint64_t));
    for (int i = 256; i < 512; i++) {
        pml4_virt[i] = kernel_pml4[i];
    }

    // 3. Load Segments
    Elf64_Phdr* phdr = (Elf64_Phdr*)((uint8_t*)elf_data + header->e_phoff);

    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t filesz = phdr[i].p_filesz;
            uint64_t memsz = phdr[i].p_memsz;
            uint64_t vaddr = phdr[i].p_vaddr;
            uint64_t file_offset = phdr[i].p_offset;

            // Loop through the segment page-by-page
            // This handles non-contiguous physical pages automatically
            uint64_t count = (memsz + 0xFFF) / 4096;
            
            for (uint64_t j = 0; j < count; j++) {
                uint64_t offset = j * 4096;
                
                // A. Allocate a new page (returns HHDM Virtual Address)
                void* page_virt = pmm_alloc_page();
                uint64_t page_phys = get_phys_addr(page_virt);
                
                // B. Map it to User Space
                // Note: We use pml4_virt here so we can write the PTEs!
                vmm_map_page(pml4_virt, vaddr + offset, page_phys, 0x7); // User | RW | Present
                
                // C. Clear the page (for BSS safety)
                memset(page_virt, 0, 4096);

                // D. Copy data from ELF to this page
                // We calculate how much data from the file goes into *this specific page*
                if (offset < filesz) {
                    uint64_t copy_size = filesz - offset;
                    if (copy_size > 4096) copy_size = 4096;
                    
                    // Copy directly using HHDM addresses. No CR3 switch needed!
                    memcpy(page_virt, (uint8_t*)elf_data + file_offset + offset, copy_size);
                }
            }
        }
    }

    // Calculate number of pages needed for 16MB
    size_t stack_pages = USER_STACK_SIZE / 4096;
    if (USER_STACK_SIZE % 4096 != 0) {
        stack_pages++;  // Round up if not exact multiple of page size
    }

    // Allocate and map each page
    for (size_t i = 0; i < stack_pages; i++) {
        void* stack_page = pmm_alloc_page();
        if (!stack_page) {
            // Handle allocation failure - free any already allocated pages
            // You may want to add cleanup logic here
            serial_printf("No more memory to map user stack. Blowing up now fuck you!");
            return;
        }
        
        // Map page at appropriate virtual address
        // Stack grows downward, so we start from the top
        uint64_t page_vaddr = USER_STACK_TOP - ((i + 1) * 4096);
        vmm_map_page(pml4_virt, page_vaddr, get_phys_addr(stack_page), 0x7);
    }

    // Set initial stack pointer (top of allocated stack)
    uint64_t user_stack_top = USER_STACK_TOP;

    // 5. Create Task Structure
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    new_task->pid = 2;
    new_task->cr3 = (uint64_t)pml4_phys; // Store PHYSICAL address for CR3
    new_task->kernel_stack = (uint64_t)kmalloc(4096) + 4096;
    new_task->next = NULL;

    // 6. Forge the Interrupt Stack Frame
    uint64_t* sp = (uint64_t*)new_task->kernel_stack;
    
    sp--; *sp = 0x23;               // SS
    sp--; *sp = user_stack_top;     // RSP
    sp--; *sp = 0x202;              // RFLAGS
    sp--; *sp = 0x1B;               // CS
    sp--; *sp = header->e_entry;    // RIP

    for (int i=0; i<15; i++) { sp--; *sp = 0; }
    
    new_task->rsp = (uint64_t)sp;
    
    // Add to scheduler
    extern task_t* task_head;
    // Simple insert at head (or end, depending on your pref)
    task_t* next = task_head->next;
    task_head->next = new_task;
    new_task->next = next;
    
    serial_printf("Process loaded! Entry: 0x%x\n", header->e_entry);
}

void create_user_process_from_file(const char* filename, int is_wm) {
    FIL file;
    FRESULT res;
    UINT bytes_read;

    // 1. Open the ELF file
    res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        serial_printf("Failed to open file: %s (Error: %d)\n", filename, res);
        return;
    }

    // 2. Read ELF Header
    Elf64_Ehdr header;
    res = f_read(&file, &header, sizeof(Elf64_Ehdr), &bytes_read);
    
    if (res != FR_OK || bytes_read != sizeof(Elf64_Ehdr)) {
        serial_printf("Failed to read ELF header\n");
        f_close(&file);
        return;
    }

    // 3. Verify Magic
    if (header.e_ident[0] != 0x7F || header.e_ident[1] != 'E' || 
        header.e_ident[2] != 'L' || header.e_ident[3] != 'F') {
        serial_printf("Invalid ELF Magic in file\n");
        f_close(&file);
        return;
    }

    // 4. Create Process Address Space
    uint64_t* pml4_virt = pmm_alloc_page(); 
    uint64_t* pml4_phys = (uint64_t*)get_phys_addr(pml4_virt);
    
    memset(pml4_virt, 0, 256 * sizeof(uint64_t));
    for (int i = 256; i < 512; i++) {
        pml4_virt[i] = kernel_pml4[i];
    }

    // 5. Read Program Headers
    // Calculate size needed for all program headers
    uint32_t ph_size = header.e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr* phdr = (Elf64_Phdr*)kmalloc(ph_size);
    
    // Seek to the program header table in the file
    f_lseek(&file, header.e_phoff);
    f_read(&file, phdr, ph_size, &bytes_read);

    // 6. Load Segments
    for (int i = 0; i < header.e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t filesz = phdr[i].p_filesz;
            uint64_t memsz = phdr[i].p_memsz;
            uint64_t vaddr = phdr[i].p_vaddr;
            uint64_t file_offset = phdr[i].p_offset;

            // Calculate number of pages needed for this segment
            uint64_t count = (memsz + 0xFFF) / 4096;
            
            for (uint64_t j = 0; j < count; j++) {
                uint64_t offset = j * 4096;
                
                // A. Allocate a new page (returns HHDM Virtual Address)
                void* page_virt = pmm_alloc_page();
                uint64_t page_phys = get_phys_addr(page_virt);
                
                // B. Map it to User Space
                vmm_map_page(pml4_virt, vaddr + offset, page_phys, 0x7); // User | RW | Present
                
                // C. Clear the page (Important for BSS and partial pages)
                memset(page_virt, 0, 4096);

                // D. Read data from File into this page
                if (offset < filesz) {
                    uint64_t copy_size = filesz - offset;
                    if (copy_size > 4096) copy_size = 4096;
                    
                    // Seek to the correct offset in the file for THIS page
                    f_lseek(&file, file_offset + offset);
                    
                    // Read directly into the page memory
                    f_read(&file, page_virt, copy_size, &bytes_read);
                }
            }
        }
    }

    // Cleanup Program Headers buffer
    kfree(phdr);
    
    // Close the file (We are done reading)
    f_close(&file);

    // 7. Allocate User Stack (Same as before)
    size_t stack_pages = USER_STACK_SIZE / 4096;
    if (USER_STACK_SIZE % 4096 != 0) stack_pages++;

    for (size_t i = 0; i < stack_pages; i++) {
        void* stack_page = pmm_alloc_page();
        if (!stack_page) {
            serial_printf("OOM during stack allocation\n");
            return;
        }
        uint64_t page_vaddr = USER_STACK_TOP - ((i + 1) * 4096);
        vmm_map_page(pml4_virt, page_vaddr, get_phys_addr(stack_page), 0x7);
    }

    uint64_t user_stack_top = USER_STACK_TOP;

    
    // 8. Create Task Structure (Same as before)
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    new_task->pid = 2; // In a real OS, use an atomic counter or bitmap allocator
    new_task->cr3 = (uint64_t)pml4_phys;
    new_task->kernel_stack = (uint64_t)kmalloc(4096) + 4096;
    new_task->next = NULL;

    // If the task will be launched as a window manager, map framebuffer to userspace
    if (is_wm) {
        uint64_t fb_phys = get_phys_addr(framebuffer->address);

        for (uint64_t offset = 0; offset < fb_size(); offset += PAGE_SIZE) {
            vmm_map_page(pml4_virt, USER_FB_BASE + offset, fb_phys + offset, VMM_PRESENT | VMM_WRITE | VMM_USER | VMM_PCD | VMM_PWT);
        }


        new_task->is_wm = 1;
    }

    // 9. Forge the Interrupt Stack Frame
    uint64_t* sp = (uint64_t*)new_task->kernel_stack;
    
    sp--; *sp = 0x23;               // SS
    sp--; *sp = user_stack_top;     // RSP
    sp--; *sp = 0x202;              // RFLAGS
    sp--; *sp = 0x1B;               // CS
    sp--; *sp = header.e_entry;     // RIP (Note: used .e_entry not ->e_entry)

    for (int i=0; i<15; i++) { sp--; *sp = 0; }
    
    new_task->rsp = (uint64_t)sp;
    
    // Add to scheduler
    extern task_t* task_head;
    task_t* next = task_head->next;
    task_head->next = new_task;
    new_task->next = next;
    
    serial_printf("Process loaded from %s! Entry: 0x%x\n", filename, header.e_entry);
}

void task_exit(void) {
    asm volatile("cli"); // Disable interrupts immediately
    
    task_t* victim = current_task;
    
    serial_printf("Exiting PID %d...\n", victim->pid);

    // 1. Find the previous task in the list to unlink 'victim'
    task_t* prev = task_head;
    while (prev->next != victim) {
        prev = prev->next;
        // Safety check: if loop wraps around and we don't find victim
        if (prev == task_head && victim != task_head) {
             serial_printf("Panic: Corrupt task list\n");
             while(1);
        }
    }

    // 2. Unlink the victim
    prev->next = victim->next;
    
    // If we are killing the head, move the head
    if (victim == task_head) {
        task_head = victim->next;
    }

    // 3. Mark as zombie (Scheduled for deletion by the next task)
    zombie_task = victim;

    // 4. Switch to next task manually
    current_task = victim->next; // Move current pointer to next

    tss_set_rsp0(current_task->kernel_stack + 4096);

    uint64_t old_cr3 = read_cr3();
    if (current_task->cr3 != 0 && current_task->cr3 != old_cr3) {
        write_cr3(current_task->cr3);
    }

    // 5. Jump to the new stack and restore registers
    // This function DOES NOT RETURN.
    exit_switch_to(current_task->rsp);
}

// This is called by the timer handler
uint64_t scheduler_schedule(uint64_t current_rsp) {
    // 1. Clean up any zombies left by previous exit calls
    if (zombie_task != NULL) {
        // Free Kernel Stack
        kfree((void*)zombie_task->kernel_stack);
        
        // Free PML4 Page (Physical Address)
        // We calculate Virtual address to pass to pmm_free_page (if your pmm expects virt or phys)
        // pmm_free_page expects a HHDM virtual address:
        void* pml4_virt = (void*)(zombie_task->cr3 + limine_hhdm);
        pmm_free_page(pml4_virt);
        
        // Free Task Struct
        kfree(zombie_task);
        
        // Clear zombie ptr
        zombie_task = NULL;
    }
    if (!current_task) return current_rsp;

    // Save the stack pointer of the task we are LEAVING
    current_task->rsp = current_rsp;

    // Pick the NEXT task
    current_task = current_task->next;

    tss_set_rsp0(current_task->kernel_stack);

    // Read the current CR3
    uint64_t old_cr3 = read_cr3();
    
    // Only switch if it's actually different (to save performance)
    if (current_task->cr3 != 0 && current_task->cr3 != old_cr3) {
        write_cr3(current_task->cr3);
    }

    //serial_printf("Switching to PID %d\n", current_task->pid);

    // Return the stack pointer of the task we are ENTERING
    return current_task->rsp;
}