#include <syscall/syscall.h>
#include <drivers/com1.h>
#include <drivers/framebuffer.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <multitasking/scheduler.h>
#include <memory/shm.h>
#include <utility/kstring.h>
#include <fs/vfs.h>

#define ERR_SUCCESS 0
#define ERR_FAIL -1

static int alloc_fd(process_t* proc, vfs_file_t* file) {
    for (int i = 0; i < PROCESS_MAX_FDS; i++) {
        if (proc->fd_table[i] == NULL) {
            proc->fd_table[i] = file;
            return i;
        }
    }
    return -1; // Process FD table is full
}

static vfs_file_t* get_fd(process_t* proc, int fd) {
    if (fd < 0 || fd >= PROCESS_MAX_FDS) return NULL;
    return proc->fd_table[fd];
}

static int copy_user_string(char* dst, size_t dst_len, const char* src) {
    if (dst == NULL || src == NULL || dst_len == 0) {
        return ERR_FAIL;
    }

    for (size_t i = 0; i < dst_len - 1; ++i) {
        char c = src[i];
        dst[i] = c;
        if (c == '\0') {
            return ERR_SUCCESS;
        }
    }

    dst[dst_len - 1] = '\0';
    return ERR_FAIL;
}


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
            size_t current_pid = scheduler_current_pid();
            if (current_pid != 0) {
                scheduler_kill_process_tree(current_pid, (int)exit_code);
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

        case SYS_GETPID: {
            context->rax = scheduler_current_pid();
            break;
        }

        case SYS_SPAWN: {
            size_t child_pid = 0;
            const char* path = (const char*)context->rdi;
            const char** argv = (const char**)context->rsi;
            
            int st = scheduler_spawn_process(path, argv, scheduler_current_pid(), &child_pid);
            context->rax = (st == ERR_SUCCESS) ? child_pid : (uint64_t)ERR_FAIL;
            break;
        }

        case SYS_WAIT: {
            int* out_exit = (int*)context->rsi;
            context->rax = (uint64_t)scheduler_wait_process(scheduler_current_pid(), context->rdi, out_exit);
            break;
        }

        case SYS_KILL: {
            context->rax = (uint64_t)scheduler_kill_process_tree(context->rdi, 137);
            break;
        }

        case SYS_FS_OPEN: {
            const char* path = (const char*)context->rdi;
            uint32_t flags = (uint32_t)context->rsi;
            vfs_file_t* file = NULL;
            
            if (path == NULL || vfs_open(path, flags, &file) != VFS_OK || file == NULL) {
                context->rax = (uint64_t)ERR_FAIL;
                break;
            }

            process_t* current_process = scheduler_current_thread()->parent;
            int fd = alloc_fd(current_process, file);
            if (fd < 0) {
                vfs_close(file);
                context->rax = (uint64_t)ERR_FAIL;
                break;
            }

            context->rax = (uint64_t)fd;
            break;
        }

        case SYS_FS_READ: {
            int fd = (int)context->rdi;
            void* buffer = (void*)context->rsi;
            size_t bytes = (size_t)context->rdx;
            size_t* out_read = (size_t*)(uintptr_t)context->r10;
            size_t rd = 0;

            process_t* current_process = scheduler_current_thread()->parent;
            vfs_file_t* file = get_fd(current_process, fd);

            if (file == NULL || buffer == NULL || out_read == NULL) {
                context->rax = (uint64_t)ERR_FAIL;
                break;
            }

            if (vfs_read(file, buffer, bytes, &rd) != VFS_OK) {
                context->rax = (uint64_t)ERR_FAIL;
                break;
            }

            *out_read = rd;
            context->rax = ERR_SUCCESS;
            break;
        }

        case SYS_FS_CLOSE: {
            int fd = (int)context->rdi;
            process_t* current_process = scheduler_current_thread()->parent;
            vfs_file_t* file = get_fd(current_process, fd);
            
            if (file == NULL) {
                context->rax = (uint64_t)ERR_FAIL;
                break;
            }

            context->rax = (vfs_close(file) == VFS_OK) ? ERR_SUCCESS : (uint64_t)ERR_FAIL;
            current_process->fd_table[fd] = NULL;
            break;
        }

        case SYS_PIPE: {
            uint64_t* user_r = (uint64_t*)context->rdi;
            uint64_t* user_w = (uint64_t*)context->rsi;
            vfs_file_t *r_file = NULL, *w_file = NULL;
            
            if (user_r == NULL || user_w == NULL) {
                context->rax = (uint64_t)ERR_FAIL;
                break;
            }
            
            if (vfs_create_pipe(&r_file, &w_file) == VFS_OK) {
                process_t* current_process = scheduler_current_thread()->parent;
                int fd_r = alloc_fd(current_process, r_file);
                int fd_w = alloc_fd(current_process, w_file);
                
                if (fd_r < 0 || fd_w < 0) {
                    if (fd_r >= 0) current_process->fd_table[fd_r] = NULL;
                    if (fd_w >= 0) current_process->fd_table[fd_w] = NULL;
                    vfs_close(r_file);
                    vfs_close(w_file);
                    context->rax = (uint64_t)ERR_FAIL;
                } else {
                    *user_r = (uint64_t)fd_r;
                    *user_w = (uint64_t)fd_w;
                    context->rax = ERR_SUCCESS;
                }
            } else {
                context->rax = (uint64_t)ERR_FAIL;
            }
            break;
        }

        case SYS_FS_WRITE: {
            int fd = (int)context->rdi;
            void* buffer = (void*)context->rsi;
            size_t bytes = (size_t)context->rdx;
            size_t* out_written = (size_t*)(uintptr_t)context->r10;
            size_t wr = 0;

            process_t* current_process = scheduler_current_thread()->parent;
            vfs_file_t* file = get_fd(current_process, fd);

            if (file == NULL || buffer == NULL || out_written == NULL) {
                context->rax = (uint64_t)ERR_FAIL;
                break;
            }

            if (vfs_write(file, buffer, bytes, &wr) == VFS_OK) {
                *out_written = wr;
                context->rax = ERR_SUCCESS;
            } else {
                context->rax = (uint64_t)ERR_FAIL;
            }
            break;
        }

        default:
            serial_printf("[SYSCALL] Unknown syscall number: %u\n", (uint32_t)syscall_num);
            context->rax = (uint64_t)ERR_FAIL; 
            break;
    }

    return context;
}