#include <multitasking/scheduler.h>
#include <multitasking/process.h>
#include <memory/kheap.h>
#include <descriptors/gdt.h>
#include <memory/vmm.h>
#include <utility/kstring.h>
#include <memory/pmm.h>

#define PROCESS_STACK_SIZE (1024 * 1024)

thread_t* run_queue_head = NULL;
thread_t* run_queue_tail = NULL;
thread_t* zombie_queue_head = NULL;

thread_t* current_thread = NULL;
thread_t* idle_thread = NULL;

process_t* processes_list = NULL;

size_t next_tid = 1;
size_t next_pid = 1;

void enqueue_thread(thread_t* thread) {
    if (!thread) return;
    
    thread->next = NULL;
    
    if (run_queue_head == NULL) {
        run_queue_head = thread;
        run_queue_tail = thread;
    } else {
        run_queue_tail->next = thread;
        run_queue_tail = thread;
    }
}

thread_t* dequeue_thread() {
    if (run_queue_head == NULL) return NULL;
    
    thread_t* thread = run_queue_head;
    run_queue_head = run_queue_head->next;
    
    if (run_queue_head == NULL) {
        run_queue_tail = NULL;
    }
    
    return thread;
}

uint64_t alloc_stack(thread_t* thread) {
    void* stack_bottom = kmalloc(PROCESS_STACK_SIZE);
    if (stack_bottom == NULL) return 0;

    thread->stack_base = stack_bottom;
    
    uint64_t stack_top = (uint64_t)stack_bottom + PROCESS_STACK_SIZE;
    stack_top &= ~0xF;
    return stack_top;
}

cpu_status_t* schedule(cpu_status_t* context) {
    if (current_thread != NULL) {
        current_thread->context = context;

        if (current_thread->status == THREAD_RUNNING) {
            current_thread->status = THREAD_READY;
            
            if (current_thread != idle_thread) {
                enqueue_thread(current_thread);
            }
        }
        else if (current_thread->status == THREAD_DEAD) {
            current_thread->next = zombie_queue_head;
            zombie_queue_head = current_thread;
        }
    }

    thread_t* next_thread = dequeue_thread();

    if (next_thread == NULL) {
        next_thread = idle_thread;
    }

    current_thread = next_thread;
    current_thread->status = THREAD_RUNNING;

    if (current_thread->parent != NULL && current_thread->parent->pml4 != NULL) {
        phys_addr_t new_cr3 = vmm_get_phys(current_thread->parent->pml4);
        if (read_cr3() != new_cr3) {
            write_cr3(new_cr3);
        }
    }

    return current_thread->context;
}

thread_t* create_thread(process_t* parent, void(*function)(void*), void* arg) {
    thread_t* thread = kmalloc(sizeof(thread_t));
    if (!thread) return NULL;
    
    thread->tid = next_tid++;
    thread->status = THREAD_READY;
    thread->parent = parent;
    
    uint64_t stack_top = alloc_stack(thread);
    if (stack_top == 0) {
        kfree(thread);
        return NULL;
    }
    
    thread->context = (cpu_status_t*)(stack_top - sizeof(cpu_status_t));
    memset(thread->context, 0, sizeof(cpu_status_t));
    
    thread->context->iret_ss = KERNEL_DATA;
    thread->context->iret_rsp = stack_top;
    thread->context->iret_flags = 0x202;
    thread->context->iret_cs = KERNEL_CODE;
    thread->context->iret_rip = (uint64_t)function;
    thread->context->rdi = (uint64_t)arg;

    thread->sibling = parent->threads;
    parent->threads = thread;
    
    enqueue_thread(thread);
    
    return thread;
}

process_t* create_process(char* name, void(*function)(void*), void* arg) {
    process_t* process = kmalloc(sizeof(process_t));
    if (!process) return NULL;
    
    strncpy(process->name, name, PROC_NAME_LEN);
    process->pid = next_pid++;
    process->pml4 = vmm_create_new_pml4();
    process->threads = NULL;

    process->next = processes_list;
    processes_list = process;

    thread_t* main_thread = create_thread(process, function, arg);
    
    if (kstrcmp(name, "idle") == 0) {
        idle_thread = main_thread;
    }

    return process;
}