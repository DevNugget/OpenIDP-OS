#ifndef OPENIDP_H
#define OPENIDP_H

#include "libc/stdint.h"

#define SYS_WRITE 0
#define SYS_EXIT  1
#define SYS_READ_KEY 2
#define SYS_EXEC 7
#define SYS_GET_FB_INFO 8
#define SYS_SBRK 9
#define SYS_IPC_SEND 10
#define SYS_IPC_RECV 11
#define SYS_SHARE_MEM 12
#define SYS_FILE_READ 13
#define SYS_UNMAP 14

#define MSG_REQUEST_WINDOW 100 

// Messages TO Client
#define MSG_WINDOW_CREATED 200 // d1=w, d2=h, d3=buffer_ptr
#define MSG_WINDOW_RESIZE  201 // d1=w, d2=h

// Common
#define MSG_BUFFER_UPDATE  300
#define MSG_KEY_EVENT 500
#define MSG_QUIT_REQUEST  0xDEAD

struct fb_info {
    uint64_t fb_addr;
    uint64_t fb_width;
    uint64_t fb_height;
    uint64_t fb_pitch;
    uint64_t fb_bpp;
};

typedef struct {
    int sender_pid;
    int type;
    uint64_t data1;
    uint64_t data2;
    uint64_t data3;
} message_t;

static inline int sys_write(int fd, const char* buf) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYS_WRITE), "D" ((uint64_t)fd), "S" ((uint64_t)buf)
        : "memory"
    );

    return ret;
}

static inline void sys_exit(int code) {
    int ret;
        asm volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYS_EXIT), "D" ((uint64_t)code)
        : "memory"
    );
}

static inline uint16_t sys_read_key(void) {
    uint64_t ret; // Use 64-bit storage for the syscall result
    asm volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYS_READ_KEY)
        : "memory"
    );
    return (uint16_t)ret; // Cast back to the actual packet size
}

static inline int sys_exec(const char* path) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYS_EXEC), "D" ((uint64_t)path)
        : "memory"
    );
    return ret;
}

static inline int sys_get_framebuffer_info(struct fb_info* out) {
    int ret;

    asm volatile (
    "int $0x80"
    : "=a" (ret)
    : "a" (SYS_GET_FB_INFO), "D" (out)
    : "memory"
    );
    return ret;
}

static void* sbrk(intptr_t increment) {
    void* ret;
    asm volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYS_SBRK), "D" (increment) // 9 is SYS_SBRK
        : "memory"
    );
    return ret;
}

static inline int sys_ipc_send(int dest_pid, int type, uint64_t d1, uint64_t d2, uint64_t d3) {
    int ret;
    // CRITICAL: Force d3 into R8. 
    // Just using "r"(d3) in the asm list is NOT enough!
    register uint64_t r8 asm("r8") = d3;

    asm volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYS_IPC_SEND), 
          "D" ((uint64_t)dest_pid), 
          "S" ((uint64_t)type), 
          "d" (d1), 
          "c" (d2), 
          "r" (r8)  // <--- Must use the pinned variable here
        : "memory"
    );
    return ret;
}

static inline int sys_ipc_recv(message_t* msg_out) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYS_IPC_RECV), "D" (msg_out)
        : "memory"
    );
    return ret;
}

// Returns: Local Virtual Address of the shared buffer
// Output: *target_vaddr_out gets the address valid in the Target Process
static inline void* sys_share_mem(int target_pid, uint64_t size, uint64_t* target_vaddr_out) {
    void* ret;
    asm volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYS_SHARE_MEM), "D" ((uint64_t)target_pid), "S" (size), "d" (target_vaddr_out)
        : "memory"
    );
    return ret;
}

static inline int sys_file_read(const char* path, void* buf, uint64_t max_len) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYS_FILE_READ), "D" ((uint64_t)path), "S" ((uint64_t)buf), "d" (max_len)
        : "memory"
    );
    return ret;
}

static inline int sys_unmap(void* addr, uint64_t size) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYS_UNMAP), "D" ((uint64_t)addr), "S" (size)
        : "memory"
    );
    return ret;
}

#endif