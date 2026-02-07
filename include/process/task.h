#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>
#include <kheap.h>
#include <com1.h>
#include <gdt.h> // For KERNEL_CS / KERNEL_DS
#include <vmm.h>
#include <kelf.h>
#include <fatfs/ff.h>
#include <graphics.h>

#define USER_STACK_SIZE (16 * 1024 * 1024)  // 16MB
#define USER_STACK_TOP 0x700000000  // Start of user stack region
#define USER_FB_BASE 0x800000000ULL

#define PTE_PRESENT 1
#define PAGE_ADDR_MASK 0x000FFFFFFFFFF000
#define HH_START 0xFFFF800000000000

// This struct must exactly match the pushes in idt.asm
typedef struct {
    // Pushed by our irq_stub (in reverse order of push)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    // Pushed by the CPU automatically on interrupt
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

#define MSG_QUEUE_SIZE 16

typedef struct {
    int sender_pid;
    int type;
    uint64_t data1;
    uint64_t data2;
    uint64_t data3;
} message_t;

typedef struct task {
    uint64_t  rsp;          
    uint64_t  cr3;
    uint64_t  pid;
    uint64_t  kernel_stack;
    struct task* next;      

    uint64_t is_wm;
    uint64_t program_break;

    message_t msgs[MSG_QUEUE_SIZE];
    int msg_head;
    int msg_tail;
    int msg_count;
    int waiting_for_msg;
} task_t;

void scheduler_init(void);
uint64_t scheduler_schedule(uint64_t current_rsp);

void create_kernel_task(void (*entry_point)());
int create_user_process_from_file(const char* filename, int argc, char** argv, int is_wm);
void task_exit(void);

int sys_ipc_send(int dest_pid, int type, uint64_t d1, uint64_t d2, uint64_t d3);
int sys_ipc_recv(message_t* out_msg);

void copy_to_user_mem(uint64_t* user_pml4, uint64_t vaddr, void* data, uint64_t size);
task_t* get_task_by_pid(uint64_t pid);

#endif
