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
    uint64_t val = 0;
    while (1) {
        // 1. NON-BLOCKING CHECK:
        // If the current task has pending messages, return 0 immediately.
        if (current_task->msg_count > 0) {
            return 0;
        }

        // 2. Standard Keyboard Check
        asm volatile("cli");
        val = keyboard_read_key();

        if (val != 0) {
            asm volatile("sti");
            return val;
        }

        // 3. Sleep until interrupt
        asm volatile("sti; hlt");
    }
}

// Helper for Screen Properties
// prop_id: 0 = Width, 1 = Height
uint64_t sys_get_screen_prop(uint64_t prop_id) {
    if (prop_id == 0) return fb_width();
    if (prop_id == 1) return fb_height();
    return 0;
}

// Helper for Drawing
void sys_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    // In a real OS, you would clamp these values to screen bounds here
    fb_draw_filled_rect(x, y, w, h, color);
}

void sys_blit(uint32_t* user_buffer) {
    if (!framebuffer) return;

    uint64_t width = framebuffer->width;
    uint64_t height = framebuffer->height;
    uint64_t pitch = framebuffer->pitch;
    uint8_t* vram = (uint8_t*)framebuffer->address;
    
    // User buffer is assumed to be tightly packed (Width * 4 bytes per row)
    // VRAM might have padding (Pitch)
    
    // Optimization: If no padding, copy everything at once
    if (pitch == width * 4) {
        memcpy(vram, user_buffer, width * height * 4);
    } else {
        // Copy row by row
        for (uint64_t y = 0; y < height; y++) {
            // Dest: vram + (y * pitch)
            // Src:  user_buffer + (y * width)
            // Size: width * 4 bytes
            memcpy(
                vram + (y * pitch), 
                user_buffer + (y * width), 
                width * 4
            );
        }
    }
}

void sys_exec(const char* path) {
    create_user_process_from_file(path, 0);
}

void sys_draw_string(char* c, int x, int y, int fg, int bg) {
    // Uses the kernel's font renderer
    fb_draw_string(c, x, y, fg, bg, USE_PSF2);
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

    /*
    serial_printf("Userout: %x\n", user_out);
    uint64_t old_cr3 = read_cr3();
    serial_printf("Old CR3: %x\n", old_cr3);
    write_cr3((uint64_t)current_task->cr3);
    serial_printf("PID: %x\n", current_task->pid);
    serial_printf("New CR3: %x\n", current_task->cr3);
    memcpy(user_out, &info, sizeof(info));
    write_cr3(old_cr3);
    */
    memcpy(user_out, &info, sizeof(info));
    return 0;
}

// inc: Amount to increase heap (in bytes). If 0, just return current break.
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
            return sys_read_key();

        case SYS_GET_SCREEN_PROP:
            return sys_get_screen_prop(regs->rdi);

        case SYS_DRAW_RECT:
            // RDI=x, RSI=y, RDX=w, RCX=h, R8=color
            sys_draw_rect(
                (uint32_t)regs->rdi, 
                (uint32_t)regs->rsi, 
                (uint32_t)regs->rdx, 
                (uint32_t)regs->rcx, 
                (uint32_t)regs->r8
            );
            return 0;
        
        case SYS_BLIT:
            // RDI holds the pointer to the user's backbuffer
            sys_blit((uint32_t*)regs->rdi);
            return 0;

        case SYS_DRAW_STRING: // SYS_DRAW_CHAR: rdi=char, rsi=x, rdx=y, rcx=fg, r8=bg
             sys_draw_string((char*)regs->rdi, (int)regs->rsi, (int)regs->rdx, (int)regs->rcx, (int)regs->r8);
             return 0;
             
        case SYS_EXEC: // SYS_EXEC: rdi=filename_ptr
             sys_exec((const char*)regs->rdi);
             return 0;

        case SYS_GET_FB_INFO:
            return sys_get_fb_info((struct fb_info*)regs->rdi);

        case SYS_SBRK:
            return (uint64_t)sys_sbrk((intptr_t)regs->rdi);

        case SYS_IPC_SEND:
            // Arguments: RDI=dest_pid, RSI=type, RDX=d1, RCX=d2, R8=d3
            return sys_ipc_send(
                (int)regs->rdi, 
                (int)regs->rsi, 
                regs->rdx, 
                regs->rcx, 
                regs->r8
            );

        case SYS_IPC_RECV:
            // Arguments: RDI = pointer to message_t struct in user memory
            return sys_ipc_recv((message_t*)regs->rdi);

        case SYS_SHARE_MEM:
            // Arguments: RDI=target_pid, RSI=size, RDX=pointer to output variable (target_vaddr)
            return sys_share_mem(
                (int)regs->rdi,
                (size_t)regs->rsi,
                (uint64_t*)regs->rdx
            );

        default:
            serial_printf("[KERNEL] Unknown Syscall: %d\n", syscall_number);
            return -1;
    }
}