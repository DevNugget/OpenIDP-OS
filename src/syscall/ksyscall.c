#include <ksyscall.h>

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

        default:
            serial_printf("[KERNEL] Unknown Syscall: %d\n", syscall_number);
            return -1;
    }
}