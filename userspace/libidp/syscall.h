#ifndef LIBIDP_SYSCALL_H
#define LIBIDP_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#define SYS_YIELD 0
#define SYS_PRINT 1
#define SYS_EXIT  2
#define SYS_SHM_CREATE 3
#define SYS_SHM_MAP 4
#define SYS_SHM_UNMAP 5
#define SYS_SHM_DESTROY 6
#define SYS_FRAMEBUFFER_GET_INFO 7
#define SYS_KEYBOARD_POLL 8
#define SYS_KEYBOARD_READ 9
#define SYS_MOUSE_POLL 10
#define SYS_MOUSE_READ 11
#define SYS_GETPID 12
#define SYS_SPAWN 13
#define SYS_WAIT 14
#define SYS_KILL 15
#define SYS_FS_OPEN 16
#define SYS_FS_READ 17
#define SYS_FS_CLOSE 18
#define SYS_PIPE 19
#define SYS_FS_WRITE 20
#define SYS_SYSINFO 21
#define SYS_PROC_LIST 22
#define SYS_DUP2 23
#define SYS_FS_READDIR 24

#define ERR_SUCCESS 0
#define ERR_FAIL   -1

#define IDP_O_RDONLY    0x1
#define IDP_O_WRONLY    0x2
#define IDP_O_CREATE    0x4
#define IDP_O_DIRECTORY 0x8

#define IDP_DIRENT_TYPE_FILE 1
#define IDP_DIRENT_TYPE_DIR  2
#define IDP_DIRENT_NAME_MAX 64

typedef struct idp_dirent_t {
    uint8_t type;
    char name[IDP_DIRENT_NAME_MAX];
} idp_dirent_t;

typedef struct framebuffer_user_info_t {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint64_t size_bytes;
    uint64_t shm_handle;
} framebuffer_user_info_t;

typedef struct mouse_event_user_t {
    int16_t delta_x;
    int16_t delta_y;
    uint8_t buttons;
} mouse_event_user_t;

#define CTRL_MASK  (1u << 0)
#define ALT_MASK   (1u << 1)
#define SHIFT_MASK (1u << 2)
#define CAPS_MASK  (1u << 3)

typedef enum {
    KEY_UNKNOWN = 0,

    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H,
    KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P,
    KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X,
    KEY_Y, KEY_Z,

    KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,

    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, 
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,

    KEY_MINUS,      
    KEY_EQUAL,      
    KEY_LBRACKET,   
    KEY_RBRACKET,   
    KEY_SEMICOLON,  
    KEY_QUOTE,      
    KEY_BACKTICK,   
    KEY_BACKSLASH,  
    KEY_COMMA,      
    KEY_DOT,        
    KEY_SLASH,      

    KEY_BACKSPACE,
    KEY_ENTER,
    KEY_TAB,
    KEY_SPACE,
    KEY_ESC,
    
    KEY_LSHIFT, KEY_RSHIFT,
    KEY_LCTRL,  KEY_RCTRL,
    KEY_LALT,   KEY_RALT,
    
    KEY_CAPSLOCK,
    KEY_NUMLOCK,
    KEY_SCROLLLOCK,

    KEY_INSERT, KEY_DELETE,
    KEY_HOME,   KEY_END,
    KEY_PGUP,   KEY_PGDN,
    KEY_UP,     KEY_DOWN, 
    KEY_LEFT,   KEY_RIGHT,

    KEY_KP_0, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4,
    KEY_KP_5, KEY_KP_6, KEY_KP_7, KEY_KP_8, KEY_KP_9,
    KEY_KP_ADD, KEY_KP_SUB, KEY_KP_MUL, KEY_KP_DIV, 
    KEY_KP_DECIMAL, KEY_KP_ENTER
} keycode_t;

typedef struct key_event_t {
    uint8_t code;
    uint8_t status_mask;
    uint8_t is_pressed;
} key_event_t;

typedef struct sysinfo_t {
    uint64_t uptime_ms;
    uint64_t total_ram;
    uint64_t free_ram;
    uint32_t procs;
    uint32_t cpus;
} sysinfo_t;

#define PROC_NAME_LEN 64

typedef struct process_user_info_t {
    uint64_t pid;
    uint64_t parent_pid;
    uint32_t exited;
    uint32_t thread_count;
    uint32_t running_thread_count;
    uint32_t running_tid_count;
    uint64_t cpu_mask;
    uint64_t running_tids[8];
    uint64_t running_cpus[8];
    char name[64];
} process_user_info_t;

static inline uint64_t syscall_0(uint64_t syscall_num) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (syscall_num)
        : "memory"
    );
    return ret;
}

static inline uint64_t syscall_1(uint64_t syscall_num, uint64_t arg1) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (syscall_num), "D" (arg1)
        : "memory"
    );
    return ret;
}

static inline uint64_t syscall_2(uint64_t syscall_num, uint64_t arg1, uint64_t arg2) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (syscall_num), "D" (arg1), "S" (arg2)
        : "memory"
    );
    return ret;
}

static inline uint64_t syscall_3(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (syscall_num), "D" (arg1), "S" (arg2), "d" (arg3)
        : "memory"
    );
    return ret;
}

static inline uint64_t syscall_4(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    uint64_t ret;
    register uint64_t r10 __asm__("r10") = arg4;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (syscall_num), "D" (arg1), "S" (arg2), "d" (arg3), "r" (r10)
        : "memory"
    );
    return ret;
}

static inline int sys_yield(void) {
    return (int)syscall_0(SYS_YIELD);
}

static inline int sys_print(const char* str) {
    return (int)syscall_1(SYS_PRINT, (uint64_t)str);
}

static inline void sys_exit(int code) {
    syscall_1(SYS_EXIT, (uint64_t)code);
    
    while (1) {
        __asm__ volatile("pause");
    }
}

static inline int64_t sys_shm_create(uint64_t size_bytes) {
    return (int64_t)syscall_1(SYS_SHM_CREATE, size_bytes);
}

static inline void* sys_shm_map(uint64_t handle) {
    return (void*)syscall_1(SYS_SHM_MAP, handle);
}

static inline int sys_shm_unmap(void* address) {
    return (int)syscall_1(SYS_SHM_UNMAP, (uint64_t)address);
}

static inline int sys_shm_destroy(uint64_t handle) {
    return (int)syscall_1(SYS_SHM_DESTROY, handle);
}

static inline int sys_framebuffer_get_info(framebuffer_user_info_t* out_info) {
    return (int)syscall_1(SYS_FRAMEBUFFER_GET_INFO, (uint64_t)out_info);
}

static inline int sys_keyboard_poll(void) {
    return (int)syscall_0(SYS_KEYBOARD_POLL);
}

static inline int sys_keyboard_read(key_event_t* out_event) {
    return (int)syscall_1(SYS_KEYBOARD_READ, (uint64_t)out_event);
}

static inline int sys_mouse_poll(void) {
    return (int)syscall_0(SYS_MOUSE_POLL);
}

static inline int sys_mouse_read(mouse_event_user_t* out_event) {
    return (int)syscall_1(SYS_MOUSE_READ, (uint64_t)out_event);
}

static inline int sys_getpid(void) {
    return (int)syscall_0(SYS_GETPID);
}

static inline int sys_spawn(const char* path, const char** argv) {
    return (int)syscall_2(SYS_SPAWN, (uint64_t)path, (uint64_t)argv);
}

static inline int sys_wait(int pid, int* out_exit_code) {
    return (int)syscall_2(SYS_WAIT, (uint64_t)pid, (uint64_t)out_exit_code);
}

static inline int sys_kill(int pid) {
    return (int)syscall_1(SYS_KILL, (uint64_t)pid);
}

static inline uint64_t sys_open(const char* path, uint32_t flags) {
    return (uint64_t)syscall_2(SYS_FS_OPEN, (uint64_t)path, (uint64_t)flags);
}

static inline int sys_readdir(uint64_t fd, idp_dirent_t* out_entry) {
    return (int)syscall_2(SYS_FS_READDIR, fd, (uint64_t)out_entry);
}

static inline int sys_read(uint64_t fd, void* buffer, uint64_t bytes, uint64_t* out_read) {
    return (int)syscall_4(SYS_FS_READ, fd, (uint64_t)buffer, bytes, (uint64_t)out_read);
}

static inline int sys_close(uint64_t fd) {
    return (int)syscall_1(SYS_FS_CLOSE, fd);
}

static inline int sys_pipe(uint64_t* read_fd, uint64_t* write_fd) {
    return (int)syscall_2(SYS_PIPE, (uint64_t)read_fd, (uint64_t)write_fd);
}

static inline int sys_write(uint64_t fd, const void* buffer, uint64_t bytes, uint64_t* out_written) {
    return (int)syscall_4(SYS_FS_WRITE, fd, (uint64_t)buffer, bytes, (uint64_t)out_written);
}

static inline int sys_info(sysinfo_t* info) {
    return (int)syscall_1(SYS_SYSINFO, (uint64_t)info);
}

static inline int sys_proc_list(process_user_info_t* out_entries, uint64_t capacity, uint64_t* out_count) {
    return (int)syscall_3(SYS_PROC_LIST, (uint64_t)out_entries, capacity, (uint64_t)out_count);
}

static inline int sys_dup2(uint64_t oldfd, uint64_t newfd) {
    return (int)syscall_2(SYS_DUP2, oldfd, newfd);
}

#endif