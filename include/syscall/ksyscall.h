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

void syscall_init(void);
uint64_t syscall_dispatcher(registers_t* regs);

#endif