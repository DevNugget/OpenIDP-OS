#include <ksyscall.h>

extern struct limine_framebuffer* framebuffer;

// Helper to write to serial console
void sys_write(int fd, const char* buf, uint64_t count) {
    // In a real OS, check 'fd' (1 = stdout) and verify 'buf' is in user memory.
    // For now, we trust the user pointer (DANGEROUS but simple for hobby OS)
    // We assume the buffer is a string, but we should strictly print 'count' bytes.
    
    (void)fd; // Unused for now, always write to COM1
    
    for (uint64_t i = 0; i < count; i++) {
        serial_write_char(buf[i]);
    }
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

// This function is called from assembly
// Returns: The value to be put in RAX (return value)
uint64_t syscall_dispatcher(registers_t* regs) {
    uint64_t syscall_number = regs->rax; // RAX holds the syscall number

    // SysV ABI for arguments: RDI, RSI, RDX, RCX, R8, R9
    switch (syscall_number) {
        case SYS_WRITE:
            sys_write((int)regs->rdi, (const char*)regs->rsi, regs->rdx);
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

        default:
            serial_printf("[KERNEL] Unknown Syscall: %d\n", syscall_number);
            return -1;
    }
}