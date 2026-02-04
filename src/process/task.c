#include <task.h>

extern void exit_switch_to(uint64_t rsp);
extern uint64_t* kernel_pml4; 
extern uint64_t limine_hhdm;

extern struct limine_framebuffer* framebuffer;

void create_kernel_task(void (*entry_point)());
void create_user_process_from_file(const char* filename, int is_wm);
void task_exit(void);
void free_page_table_level(uint64_t* table_virt, int level);
void destroy_user_memory(uint64_t pml4_phys);

task_t* current_task = NULL;
task_t* task_head = NULL;
static uint64_t next_pid = 1;

static task_t* zombie_task = NULL;

static inline uint64_t get_phys_addr(void* addr) {
    return (uint64_t)addr - limine_hhdm;
}

static inline void* get_virt_addr(uint64_t phys) {
    return (void*)(phys + limine_hhdm);
}

/* Scheduler functions */

void scheduler_init(void) {
    task_t* root_task = (task_t*)kmalloc(sizeof(task_t));
    root_task->pid = 0;
    root_task->next = root_task; 
    root_task->rsp = 0; 

    current_task = root_task;
    task_head = root_task;
    
    serial_printf("Scheduler initialized. Root task PID 0 created.\n");
}

uint64_t scheduler_schedule(uint64_t current_rsp) {
    // Clean up any zombies left by previous exit calls
    if (zombie_task != NULL) {
        // Free Kernel Stack
        kfree((void*)zombie_task->kernel_stack - 4096);

        destroy_user_memory(zombie_task->cr3);
        
        // Free PML4 Page
        void* pml4_virt = (void*)(zombie_task->cr3 + limine_hhdm);
        pmm_free_page(pml4_virt);
        
        // Free Task Struct
        kfree(zombie_task);
        
        // Clear zombie ptr
        zombie_task = NULL;
    }

    if (!current_task) return current_rsp;

    // Save the stack pointer of the task we are leaving
    current_task->rsp = current_rsp;

    // Pick the next task
    current_task = current_task->next;

    tss_set_rsp0(current_task->kernel_stack);

    uint64_t old_cr3 = read_cr3();
    
    // Only switch if it's actually different 
    if (current_task->cr3 != 0 && current_task->cr3 != old_cr3) {
        write_cr3(current_task->cr3);
    }

    // Return the stack pointer of the task we are entering
    return current_task->rsp;
}

/* Process creation functions */

void create_kernel_task(void (*entry_point)()) {
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    
    uint64_t stack_bottom = (uint64_t)kmalloc(4096);
    uint64_t stack_top = stack_bottom + 4096;

    uint64_t* sp = (uint64_t*)stack_top;
    
    sp--; *sp = KERNEL_DS;          
    sp--; *sp = stack_top;     
    sp--; *sp = 0x202; // RFLAGS (Interrupts Enabled | Reserved bit)
    sp--; *sp = KERNEL_CS;          
    sp--; *sp = (uint64_t)entry_point; // RIP

    for (int i = 0; i < 15; i++) {
        sp--; *sp = 0;
    }

    // Create task struct
    new_task->rsp = (uint64_t)sp;
    new_task->pid = next_pid++;
    new_task->cr3 = (uint64_t)vmm_create_process_pml4(kernel_pml4);
    
    // Add to linked list
    new_task->next = task_head->next;
    task_head->next = new_task;
    
    serial_printf("Task created: PID %d\n", new_task->pid);
}

void create_user_process_from_file(const char* filename, int is_wm) {
    uint64_t* pml4_virt = pmm_alloc_page(); 
    uint64_t* pml4_phys = (uint64_t*)get_phys_addr(pml4_virt);
    
    memset(pml4_virt, 0, 256 * sizeof(uint64_t));
    for (int i = 256; i < 512; i++) {
        pml4_virt[i] = kernel_pml4[i];
    }

    elf_load_result_t elf;
    load_elf_file(filename, pml4_virt, &elf);

    // Allocate user stack 
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

    
    // Create task struct 
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    memset(new_task, 0, sizeof(task_t));

    new_task->pid = next_pid++;
    new_task->cr3 = (uint64_t)pml4_phys;
    new_task->kernel_stack = (uint64_t)kmalloc(4096*4) + 4096;
    if (!new_task->kernel_stack) {
        serial_printf("OOM when kernel stack\n");
    }

    new_task->next = NULL;
    new_task->program_break = elf.program_break;

    // If the task will be launched as a window manager, map framebuffer to userspace
    if (is_wm) {
        uint64_t fb_phys = get_phys_addr(framebuffer->address);

        for (uint64_t offset = 0; offset < fb_size(); offset += PAGE_SIZE) {
            vmm_map_page(
		pml4_virt, 
		USER_FB_BASE + offset, 
		fb_phys + offset, 
		VMM_PRESENT | VMM_WRITE | VMM_USER | VMM_PCD | VMM_PWT);
        }


        new_task->is_wm = 1;
    }

    // Create interrupt stack frame 
    uint64_t* sp = (uint64_t*)new_task->kernel_stack;
    
    sp--; *sp = 0x23;               // SS
    sp--; *sp = user_stack_top;     // RSP
    sp--; *sp = 0x202;              // RFLAGS
    sp--; *sp = 0x1B;               // CS
    sp--; *sp = elf.entry;     // RIP (Note: used .e_entry not ->e_entry)

    for (int i=0; i<15; i++) { sp--; *sp = 0; }
    
    new_task->rsp = (uint64_t)sp;
    
    // Add task to linked list
    new_task->next = task_head->next;
    task_head->next = new_task;
    
    serial_printf("Process loaded from %s! Entry: 0x%x\n", filename, elf.entry);
}

/* Process freeing functions */

void task_exit(void) {
    asm volatile("cli"); // Disable interrupts

    task_t* victim = current_task;
    
    serial_printf("Exiting PID %d...\n", victim->pid);

    // Find the previous task in the list to unlink victim
    task_t* prev = task_head;
    while (prev->next != victim) {
        prev = prev->next;
        if (prev == task_head && victim != task_head) {
             serial_printf("Panic: Corrupt task list\n");
             while(1);
        }
    }

    // Unlink the victim
    prev->next = victim->next;
    
    if (victim == task_head) {
        task_head = victim->next;
    }

    zombie_task = victim; // Scheduled for deletion

    // Switch to next task
    current_task = victim->next; 

    tss_set_rsp0(current_task->kernel_stack + 4096);

    uint64_t old_cr3 = read_cr3();
    if (current_task->cr3 != 0 && current_task->cr3 != old_cr3) {
        write_cr3(current_task->cr3);
    }

    // Jump to the new stack and restore registers (switch.asm)
    exit_switch_to(current_task->rsp);
}

void free_page_table_level(uint64_t* table_virt, int level) {
    for (int i = 0; i < 512; i++) {
        uint64_t entry = table_virt[i];
        
        // Skip empty entries
        if (!(entry & PTE_PRESENT)) continue;

        uint64_t child_phys = entry & PAGE_ADDR_MASK;
        uint64_t* child_virt = (uint64_t*)get_virt_addr(child_phys);

        if (level > 1) {
            // If we are at PML4, PDP, or PD, recurse deeper
            free_page_table_level(child_virt, level - 1);
        } 
        else if (level == 1) {
            // We are at the Page Table (PT). This entry points to a physical page of RAM.
            pmm_free_page((void*)child_virt); 
        }

        // We have processed this child (or page). 
        // Now free the structure itself (The PT, PD, or PDP page)
        // We need to free the TABLE we just recursed into.
        if (level > 1) {
             pmm_free_page((void*)child_virt);
        }
        
        // Clear the entry
        table_virt[i] = 0;
    }
}

void destroy_user_memory(uint64_t pml4_phys) {
    uint64_t* pml4_virt = (uint64_t*)get_virt_addr(pml4_phys);

    // ONLY clear the lower half (User Space: 0 to 255)
    // Touching 256+ will unmap the kernel.
    for (int i = 0; i < 256; i++) {
        uint64_t entry = pml4_virt[i];
        if (entry & PTE_PRESENT) {
            uint64_t pdp_phys = entry & PAGE_ADDR_MASK;
            uint64_t* pdp_virt = (uint64_t*)get_virt_addr(pdp_phys);
            
            // Start recursion from PDP (Level 3)
            free_page_table_level(pdp_virt, 3);
            
            // Free the PDP table itself
            pmm_free_page(pdp_virt);
            
            pml4_virt[i] = 0;
        }
    }
}

/* Syscall functions */

int sys_ipc_send(int dest_pid, int type, uint64_t d1, uint64_t d2, uint64_t d3) {
    if (type == 200) { // MSG_WINDOW_CREATED
        serial_printf("[KERNEL] IPC Send. d3 (Buffer Ptr) = 0x%x\n", d3);
    }
    task_t* target = get_task_by_pid(dest_pid);
    if (!target) return -1;

    if (target->msg_count >= MSG_QUEUE_SIZE) return -2; // Queue full

    message_t* msg = &target->msgs[target->msg_tail];
    msg->sender_pid = current_task->pid;
    msg->type = type;
    msg->data1 = d1;
    msg->data2 = d2;
    msg->data3 = d3;

    target->msg_tail = (target->msg_tail + 1) % MSG_QUEUE_SIZE;
    target->msg_count++;
    
    //serial_printf("IPC_SEND: %x, %x\n", current_task->pid, type);
    return 0;
}

int sys_ipc_recv(message_t* out_msg) {
    // If empty, return -1 
    //serial_printf("IPC_RECV CURR PID: %x, %x\n", current_task->pid, current_task->msg_count);
    if (current_task->msg_count == 0) return -1;

    *out_msg = current_task->msgs[current_task->msg_head];
    current_task->msg_head = (current_task->msg_head + 1) % MSG_QUEUE_SIZE;
    current_task->msg_count--;
    //serial_printf("IPC_RECV\n");
    return 0;
}

/* Helper functions */

void copy_to_user_mem(uint64_t* user_pml4, uint64_t vaddr, void* data, uint64_t size) {
    uint64_t old_cr3 = read_cr3();
    write_cr3((uint64_t)user_pml4);
    memcpy((void*)vaddr, data, size);
    write_cr3(old_cr3);
}

task_t* get_task_by_pid(uint64_t pid) {
    task_t* curr = task_head;
    do {
        if (curr->pid == pid) return curr;
        curr = curr->next;
    } while (curr != task_head);
    return NULL;
}
