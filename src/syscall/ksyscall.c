/* 
TODO: Clean up this mess of a file.
*/

#include <ksyscall.h>

extern struct limine_framebuffer* framebuffer;
extern task_t* current_task;
extern uint64_t limine_hhdm;

// Helper to write to serial console
void sys_write(int fd, const char* buf) {
    // In a real OS, check 'fd' (1 = stdout) and verify 'buf' is in user memory.
    // For now, we trust the user pointer (DANGEROUS but simple for hobby OS)
    // We assume the buffer is a string, but we should strictly print 'count' bytes.
    
    (void)fd; // Unused for now, always write to COM1
    serial_printf(buf);
}

void sys_exit(int code) {
    serial_printf("[KERNEL] Process exited with code %d\n", code);
    
    task_exit();

    while(1) {
        serial_printf("Syscall exit error, should not reach here! HANGING\n");
        asm volatile("hlt");
    }
}

uint64_t sys_read_key() {
    // Optimization: This function is now strictly NON-BLOCKING.
    // It is up to the user program to yield/sleep if it has nothing to do.
    
    // 1. Check current task messages first (Priority to IPC)
    if (current_task->msg_count > 0) {
        return 0;
    }

    // 2. Check Hardware Keyboard Buffer
    asm volatile("cli");
    uint64_t val = keyboard_read_key(); // This must be non-blocking in your driver
    asm volatile("sti");

    return val;
}

// Helper for Screen Properties
// prop_id: 0 = Width, 1 = Height
uint64_t sys_get_screen_prop(uint64_t prop_id) {
    if (prop_id == 0) return fb_width();
    if (prop_id == 1) return fb_height();
    return 0;
}

void sys_exec(const char* path) {
    serial_printf("SYS_EXEC_PATH: %s\n", path);
    create_user_process_from_file(path, 0);
}

struct fb_info {
    uint64_t fb_addr;
    uint64_t fb_width;
    uint64_t fb_height;
    uint64_t fb_pitch;
    uint64_t fb_bpp;
};

int sys_get_fb_info(struct fb_info* user_out) {
    task_t* t = current_task;

    if (!t->is_wm) {
        return -1;
    }

    struct fb_info info;
    info.fb_addr = USER_FB_BASE;
    info.fb_width = fb_width();
    info.fb_height = fb_height();
    info.fb_pitch = fb_pitch();
    info.fb_bpp = fb_bpp();

    memcpy(user_out, &info, sizeof(info));
    return 0;
}

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

static inline uint64_t* phys_to_virt(uint64_t phys) {
    return (uint64_t*)(phys + limine_hhdm);
}

// Helper: Convert kernel virtual address to physical
static inline uint64_t virt_to_phys(void* virt) {
    return (uint64_t)virt - limine_hhdm;
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

int64_t sys_file_read(const char* path, void* user_buf, uint64_t max_len) {
    FIL file;
    FRESULT res;
    UINT bytes_read;

    // 1. Open File
    res = f_open(&file, path, FA_READ);
    if (res != FR_OK) return -1;

    // 2. Check Size
    uint64_t fsize = f_size(&file);
    if (fsize > max_len) fsize = max_len;

    // 3. Read
    // Note: In a real OS, verify user_buf is valid user memory!
    res = f_read(&file, user_buf, fsize, &bytes_read);
    f_close(&file);

    if (res != FR_OK) return -1;
    return bytes_read;
}

static inline void invlpg(void* m) {
    asm volatile("invlpg (%0)" :: "r"(m) : "memory");
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

// This function is called from assembly
// Returns: The value to be put in RAX (return value)
uint64_t syscall_dispatcher(registers_t* regs) {
    uint64_t syscall_number = regs->rax; // RAX holds the syscall number

    // SysV ABI for arguments: RDI, RSI, RDX, RCX, R8, R9
    switch (syscall_number) {
        case SYS_WRITE:
            sys_write((int)regs->rdi, (const char*)regs->rsi);
            return regs->rdx; // Return bytes written

        case SYS_EXIT:
            sys_exit((int)regs->rdi);
            return 0;

        case SYS_READ_KEY:
            //serial_printf("SYSCALL NUM: %d\n", syscall_number);
            return sys_read_key();

        case SYS_GET_SCREEN_PROP:
            return sys_get_screen_prop(regs->rdi);

        case SYS_EXEC: // SYS_EXEC: rdi=filename_ptr
             serial_printf("SYSCALL NUM: %d\n", syscall_number);
             sys_exec((const char*)regs->rdi);
             return 0;

        case SYS_GET_FB_INFO:
            return sys_get_fb_info((struct fb_info*)regs->rdi);

        case SYS_SBRK:
            serial_printf("SYSCALL NUM: %d\n", syscall_number);
            return (uint64_t)sys_sbrk((intptr_t)regs->rdi);

        case SYS_IPC_SEND:
            //serial_printf("SYSCALL NUM: %d\n", syscall_number);
            // Arguments: RDI=dest_pid, RSI=type, RDX=d1, RCX=d2, R8=d3
            return sys_ipc_send(
                (int)regs->rdi, 
                (int)regs->rsi, 
                regs->rdx, 
                regs->rcx, 
                regs->r8
            );

        case SYS_IPC_RECV:
            //serial_printf("SYSCALL NUM: %d\n", syscall_number);
            // Arguments: RDI = pointer to message_t struct in user memory
            return sys_ipc_recv((message_t*)regs->rdi);

        case SYS_SHARE_MEM:
            serial_printf("SYSCALL NUM: %d\n", syscall_number);
            // Arguments: RDI=target_pid, RSI=size, RDX=pointer to output variable (target_vaddr)
            return sys_share_mem(
                (int)regs->rdi,
                (size_t)regs->rsi,
                (uint64_t*)regs->rdx
            );

        case SYS_FILE_READ:
            serial_printf("SYSCALL NUM: %d\n", syscall_number);
            return sys_file_read((const char*)regs->rdi, (void*)regs->rsi, regs->rdx);

        case SYS_UNMAP:
            return sys_unmap((void*)regs->rdi, (size_t)regs->rsi);

        default:
            serial_printf("[KERNEL] Unknown Syscall: %d\n", syscall_number);
            return -1;
    }
}
