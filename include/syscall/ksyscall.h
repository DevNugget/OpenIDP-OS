#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <task.h> // for registers_t
#include <com1.h>
#include <kstring.h>

// Syscall Numbers
#define SYS_WRITE 0
#define SYS_EXIT  1

void syscall_init(void);
uint64_t syscall_dispatcher(registers_t* regs);

#endif