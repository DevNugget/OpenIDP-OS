#include <syscall/syscall.h>
#include <drivers/com1.h>
#include <drivers/framebuffer.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <multitasking/scheduler.h>
#include <memory/shm.h>

#define ERR_SUCCESS 0
#define ERR_FAIL -1

cpu_status_t* syscall_dispatch(cpu_status_t* context) {
    uint64_t syscall_num = context->rax;

    switch (syscall_num) {
        case SYS_YIELD:  {
            context->rax = ERR_SUCCESS;
            return schedule(context);
        }

        case SYS_PRINT: {
            serial_printf("[USER] %s\n", (char*)context->rdi);
            context->rax = ERR_SUCCESS;
            break;
        }

        case SYS_EXIT: {
            uint64_t exit_code = context->rdi;
            serial_printf("[SYSCALL] Thread exited with code %d\n", exit_code);
            
            thread_t* current = scheduler_current_thread();
            if (current != NULL) {
                current->status = THREAD_DEAD;
            }
            
            return schedule(context);
        }

        case SYS_SHM_CREATE: {
            uint64_t size_bytes = context->rdi;
            uint64_t handle = 0;
            context->rax = (shm_create((size_t)size_bytes, &handle) == ERR_SUCCESS) ? handle : (uint64_t)ERR_FAIL;
            break;
        }

        case SYS_SHM_MAP: {
            thread_t* current = scheduler_current_thread();
            uint64_t mapped_address = 0;

            if (current == NULL || current->parent == NULL ||
                shm_map(current->parent, context->rdi, &mapped_address) != ERR_SUCCESS) {
                context->rax = (uint64_t)ERR_FAIL;
                break;
            }

            context->rax = mapped_address;
            break;
        }

        case SYS_SHM_UNMAP: {
            thread_t* current = scheduler_current_thread();
            if (current == NULL || current->parent == NULL) {
                context->rax = (uint64_t)ERR_FAIL;
                break;
            }

            context->rax = (uint64_t)shm_unmap(current->parent, context->rdi);
            break;
        }

        case SYS_SHM_DESTROY: {
            context->rax = (uint64_t)shm_destroy(context->rdi);
            break;
        }

        case SYS_FRAMEBUFFER_GET_INFO: {
            framebuffer_user_info_t* user_info = (framebuffer_user_info_t*)context->rdi;
            if (user_info == NULL) {
                context->rax = (uint64_t)ERR_FAIL;
                break;
            }

            framebuffer_user_info_t info;
            if (framebuffer_get_user_info(&info) != 0) {
                context->rax = (uint64_t)ERR_FAIL;
                break;
            }

            *user_info = info;
            context->rax = ERR_SUCCESS;
            break;
        }
        
        case SYS_KEYBOARD_POLL: {
            context->rax = keyboard_poll() ? 1 : 0;
            break;
        }

        case SYS_KEYBOARD_READ: {
            key_event_t* out_event = (key_event_t*)context->rdi;
            if (out_event == NULL || !keyboard_poll()) {
                context->rax = (uint64_t)ERR_FAIL;
                break;
            }

            *out_event = keyboard_read();
            context->rax = ERR_SUCCESS;
            break;
        }

        case SYS_MOUSE_POLL: {
            context->rax = mouse_poll() ? 1 : 0;
            break;
        }

        case SYS_MOUSE_READ: {
            syscall_mouse_event_t* out_event = (syscall_mouse_event_t*)context->rdi;
            if (out_event == NULL || !mouse_poll()) {
                context->rax = (uint64_t)ERR_FAIL;
                break;
            }

            mouse_event_t event = mouse_read();
            out_event->delta_x = event.delta_x;
            out_event->delta_y = event.delta_y;
            out_event->buttons = event.buttons;
            context->rax = ERR_SUCCESS;
            break;
        }

        default:
            serial_printf("[SYSCALL] Unknown syscall number: %u\n", (uint32_t)syscall_num);
            context->rax = (uint64_t)ERR_FAIL; 
            break;
    }

    return context;
}