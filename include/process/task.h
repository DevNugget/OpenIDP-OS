#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>
#include <kheap.h>
#include <com1.h>
#include <gdt.h> // For KERNEL_CS / KERNEL_DS
#include <vmm.h>
#include <kelf.h>

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
    uint64_t  cr3;
    uint64_t  pid;
    uint64_t  kernel_stack;
    struct task* next;      // Linked list for Round Robin
} task_t;

void init_scheduler(void);
void create_kernel_task(void (*entry_point)());
void create_user_process(void* elf_data);
uint64_t scheduler_schedule(uint64_t current_rsp);

void task_exit(void);

#endif