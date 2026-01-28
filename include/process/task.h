#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>

// This struct must exactly match the pushes in idt.asm
typedef struct {
    // Pushed by our irq_stub (in reverse order of push)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    // Pushed by the CPU automatically on interrupt
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

typedef struct task {
    uint64_t  rsp;          // The stack pointer we save/restore
    uint64_t  pid;
    struct task* next;      // Linked list for Round Robin
} task_t;

void init_scheduler(void);
void create_kernel_task(void (*entry_point)());
uint64_t scheduler_schedule(uint64_t current_rsp);

#endif