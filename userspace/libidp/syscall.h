#ifndef LIBIDP_SYSCALL_H
#define LIBIDP_SYSCALL_H

#include <stdint.h>

#define SYS_YIELD 0
#define SYS_PRINT 1
#define SYS_EXIT  2
#define SYS_SHM_CREATE 3
#define SYS_SHM_MAP 4
#define SYS_SHM_UNMAP 5
#define SYS_SHM_DESTROY 6
#define SYS_FRAMEBUFFER_GET_INFO 7

#define ERR_SUCCESS 0
#define ERR_FAIL   -1

typedef struct framebuffer_user_info_t {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint64_t size_bytes;
    uint64_t shm_handle;
} framebuffer_user_info_t;

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

#endif