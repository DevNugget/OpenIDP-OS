#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <task.h> // for registers_t
#include <com1.h>
#include <kstring.h>
#include <graphics.h>
#include <keyboard.h>

// Syscall Numbers
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
#define SYS_STAT 15
#define SYS_DIR_READ 16

void syscall_init(void);
uint64_t syscall_dispatcher(registers_t* regs);

/* File-system syscalls */
struct kstat {
    uint64_t size;
    uint32_t flags; // 1 = Directory, 0 = File
};

struct kdirent {
    char name[128];   
    uint64_t size;    
    uint8_t is_dir;   // 1 if directory, 0 if file
};

int64_t sys_file_read(const char* path, void* user_buf, uint64_t max_len);
int sys_stat(const char* user_path, struct kstat* user_stat_out);
int sys_read_dir_entry(const char* path, uint64_t index, struct kdirent* user_out);

/* Framebuffer syscalls */
struct fb_info {
    uint64_t fb_addr;
    uint64_t fb_width;
    uint64_t fb_height;
    uint64_t fb_pitch;
    uint64_t fb_bpp;
};

int sys_get_fb_info(struct fb_info* user_out);

/* I/O syscalls */
void sys_write(int fd, const char* buf);
uint64_t sys_read_key();

/* Memory syscalls */
void* sys_sbrk(intptr_t inc);
uint64_t sys_share_mem(int target_pid, size_t size, uint64_t* target_vaddr_out);
int sys_unmap(void* vaddr_ptr, size_t size);

/* Process/task syscalls */
int sys_exec(const char* path, int argc, char** argv);
void sys_exit(int code);
int sys_ipc_send(int dest_pid, int type, uint64_t d1, uint64_t d2, uint64_t d3);
int sys_ipc_recv(message_t* out_msg);

#endif