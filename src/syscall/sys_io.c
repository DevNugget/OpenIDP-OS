#include <ksyscall.h>

extern task_t* current_task;

void sys_write(int fd, const char* buf) {
    (void)fd; // Unused for now, always write to COM1
    serial_printf(buf);
}

uint64_t sys_read_key() {
    if (current_task->msg_count > 0) {
        return 0;
    }

    asm volatile("cli");
    uint64_t val = keyboard_read_key();
    asm volatile("sti");

    return val;
}