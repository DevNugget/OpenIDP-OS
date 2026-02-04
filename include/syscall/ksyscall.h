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
#define SYS_GET_SCREEN_PROP 3
#define SYS_DRAW_RECT 4
#define SYS_BLIT 5
#define SYS_DRAW_STRING 6
#define SYS_EXEC 7
#define SYS_GET_FB_INFO 8
#define SYS_SBRK 9
#define SYS_IPC_SEND 10
#define SYS_IPC_RECV 11
#define SYS_SHARE_MEM 12
#define SYS_FILE_READ 13
#define SYS_UNMAP 14

void syscall_init(void);
uint64_t syscall_dispatcher(registers_t* regs);

#endif