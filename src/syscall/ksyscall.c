#include <ksyscall.h>

// This function is called from assembly descriptors/idt.asm
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

        case SYS_EXEC: 
            // RDI=filename_ptr
             return sys_exec((const char*)regs->rdi, (int)regs->rsi, (char**)regs->rdx);

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
            // RDI = pointer to message_t struct in user memory
            return sys_ipc_recv((message_t*)regs->rdi);

        case SYS_SHARE_MEM:
            // RDI=target_pid, RSI=size, RDX=pointer to output variable (target_vaddr)
            return sys_share_mem(
                (int)regs->rdi,
                (size_t)regs->rsi,
                (uint64_t*)regs->rdx
            );

        case SYS_FILE_READ:
            return sys_file_read((const char*)regs->rdi, (void*)regs->rsi, regs->rdx);

        case SYS_UNMAP:
            return sys_unmap((void*)regs->rdi, (size_t)regs->rsi);

        case SYS_STAT:
            return sys_stat((const char*)regs->rdi, (struct kstat*)regs->rsi);

        case SYS_DIR_READ:
            return sys_read_dir_entry((const char*)regs->rdi, regs->rsi, (struct kdirent*)regs->rdx);

        default:
            serial_printf("[KERNEL] Unknown Syscall: %d\n", syscall_number);
            return -1;
    }
}
