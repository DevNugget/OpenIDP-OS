#ifndef LIBIDP_SYSCALL_H
#define LIBIDP_SYSCALL_H

#include <stdint.h>

#define SYS_YIELD 0
#define SYS_PRINT 1
#define SYS_EXIT  2

#define ERR_SUCCESS 0
#define ERR_FAIL   -1

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

#endif