#include <task.h>
#include <kheap.h>
#include <com1.h>
#include <gdt.h> // For KERNEL_CS / KERNEL_DS

static task_t* current_task = NULL;
static task_t* task_head = NULL;
static uint64_t next_pid = 1;

void init_scheduler(void) {
    // Create a task wrapper for the currently running kernel code
    // so we can switch *back* to it later.
    task_t* root_task = (task_t*)kmalloc(sizeof(task_t));
    root_task->pid = 0;
    root_task->next = root_task; // Circular list
    root_task->rsp = 0; // Will be set by the first interrupt

    current_task = root_task;
    task_head = root_task;
    
    serial_printf("Scheduler initialized. Root task PID 0 created.\n");
}

void create_kernel_task(void (*entry_point)()) {
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    
    // Allocate a stack for the new task (4KB)
    uint64_t stack_bottom = (uint64_t)kmalloc(4096);
    uint64_t stack_top = stack_bottom + 4096;

    // We need to put a "fake" interrupt frame on this stack
    // so when the scheduler switches to it, iretq has something to pop.
    
    // Make room for the registers context
    uint64_t* sp = (uint64_t*)stack_top;
    
    // 1. Hardware Pushes (SS, RSP, RFLAGS, CS, RIP)
    // We strictly assume x86_64 kernel privileges for now.
    sp--; *sp = KERNEL_DS;          // SS (Kernel Data Segment) - usually 0x10 in Limine/GDT
    sp--; *sp = stack_top;     // RSP
    sp--; *sp = 0x202;         // RFLAGS (Interrupts Enabled | Reserved bit)
    sp--; *sp = KERNEL_CS;          // CS (Kernel Code Segment) - usually 0x08
    sp--; *sp = (uint64_t)entry_point; // RIP

    // 2. General Purpose Registers (R15...RAX)
    // Push 0 for all of them
    for (int i = 0; i < 15; i++) {
        sp--; *sp = 0;
    }

    // Update the task structure
    new_task->rsp = (uint64_t)sp;
    new_task->pid = next_pid++;
    
    // Add to linked list (Round Robin)
    new_task->next = task_head->next;
    task_head->next = new_task;
    
    serial_printf("Task created: PID %d\n", new_task->pid);
}

// This is called by the timer handler
uint64_t scheduler_schedule(uint64_t current_rsp) {
    if (!current_task) return current_rsp;

    // Save the stack pointer of the task we are LEAVING
    current_task->rsp = current_rsp;

    // Pick the NEXT task
    current_task = current_task->next;

    // Return the stack pointer of the task we are ENTERING
    return current_task->rsp;
}