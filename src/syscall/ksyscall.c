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
    char c = 0;
    while (1) {
        // 1. Disable interrupts so the handler can't run while we check
        asm volatile("cli");

        // 2. Check the buffer safely
        c = keyboard_read_char();

        if (c != 0) {
            // Found a key! Re-enable interrupts and return.
            asm volatile("sti");
            return (uint64_t)c;
        }

        // 3. Buffer is empty. Sleep.
        // "sti; hlt" is a special sequence. The CPU guarantees that 
        // interrupts are enabled, but the *very next* instruction (hlt) 
        // executes before any pending interrupt is handled.
        // This closes the race window perfectly.
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
    FIL file;
    FRESULT res = f_open(&file, path, FA_READ);
    if (res != FR_OK) {
        serial_printf("Exec failed: Could not open %s\n", path);
        return;
    }

    uint64_t size = f_size(&file);
    void* buffer = kmalloc(size);
    UINT bytes_read;
    
    f_read(&file, buffer, size, &bytes_read);
    f_close(&file);

    // This creates a NEW task. The current task continues running (the shell).
    create_user_process(buffer); 
    
    // Note: In a real OS, we would free 'buffer' after the process is created,
    // but your create_user_process copies data to new pages, so we can free it here.
    kfree(buffer);
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

        default:
            serial_printf("[KERNEL] Unknown Syscall: %d\n", syscall_number);
            return -1;
    }
}