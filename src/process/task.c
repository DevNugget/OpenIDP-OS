#include <task.h>

extern uint64_t* kernel_pml4; // From main.c
extern uint64_t limine_hhdm;

static task_t* current_task = NULL;
static task_t* task_head = NULL;
static uint64_t next_pid = 1;

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

    // 4. Allocate User Stack
    uint64_t user_stack_top = 0x700000000;
    void* stack_virt = pmm_alloc_page();
    vmm_map_page(pml4_virt, user_stack_top - 4096, get_phys_addr(stack_virt), 0x7);

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

// This is called by the timer handler
uint64_t scheduler_schedule(uint64_t current_rsp) {
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

    // Return the stack pointer of the task we are ENTERING
    return current_task->rsp;
}