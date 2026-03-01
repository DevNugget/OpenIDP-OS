#include <syscall/syscall.h>
#include <drivers/com1.h>
#include <multitasking/scheduler.h>

#define ERR_SUCCESS 0
#define ERR_FAIL -1

cpu_status_t* syscall_dispatch(cpu_status_t* context) {
    uint64_t syscall_num = context->rax;

    switch (syscall_num) {
        case 0: // sys_yield
            context->rax = ERR_SUCCESS;
            return schedule(context);

        case 1: // sys_print
            serial_printf("[USER] %s\n", (char*)context->rdi);
            context->rax = ERR_SUCCESS;
            break;

        case 2: { // sys_exit
            uint64_t exit_code = context->rdi;
            serial_printf("[SYSCALL] Thread exited with code %d\n", exit_code);
            
            thread_t* current = scheduler_current_thread();
            if (current != NULL) {
                current->status = THREAD_DEAD;
            }
            
            return schedule(context);
        }
        default:
            serial_printf("[SYSCALL] Unknown syscall number: %u\n", (uint32_t)syscall_num);
            context->rax = (uint64_t)ERR_FAIL; 
            break;
    }

    return context;
}