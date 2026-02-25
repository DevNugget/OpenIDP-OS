#include <multitasking/scheduler.h>
#include <multitasking/process.h>

#include <descriptors/gdt.h>

#include <drivers/apic.h>

#include <memory/kheap.h>
#include <memory/pmm.h>
#include <memory/vmm.h>

#include <utility/kstring.h>
#include <utility/spinlock.h>

#define PROCESS_STACK_SIZE (1024 * 1024)
#define SCHED_MAX_CPUS 256

extern virt_addr_t* kernel_pml4;

static thread_t* run_queue_head = NULL;
static thread_t* run_queue_tail = NULL;
static thread_t* zombie_queue_head = NULL;

static thread_t* current_threads[SCHED_MAX_CPUS] = { 0 };
static thread_t* idle_thread = NULL;

static process_t* processes_list = NULL;

static size_t next_tid = 1;
static size_t next_pid = 1;

static spinlock_t scheduler_lock = SPINLOCK_INIT;

static inline size_t cpu_slot_index(void) {
    uint32_t cpu_id = apic_get_id();
    if (cpu_id >= SCHED_MAX_CPUS) {
        return 0;
    }

    return (size_t)cpu_id;
}

thread_t* scheduler_current_thread(void) {
    thread_t* thread;
    uint64_t irq_flags = spinlock_lock_irqsave(&scheduler_lock);
    thread = current_threads[cpu_slot_index()];
    spinlock_unlock_irqrestore(&scheduler_lock, irq_flags);
    return thread;
}

static int thread_is_current_on_any_cpu(const thread_t* thread) {
    for (size_t i = 0; i < SCHED_MAX_CPUS; ++i) {
        if (current_threads[i] == thread) {
            return 1;
        }
    }
    return 0;
}

static void enqueue_thread_unsafe(thread_t* thread) {
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

static thread_t* dequeue_thread_unsafe(void) {
    if (run_queue_head == NULL) return NULL;

    thread_t* thread = run_queue_head;
    run_queue_head = run_queue_head->next;
    thread->next = NULL;

    if (run_queue_head == NULL) {
        run_queue_tail = NULL;
    }

    return thread;
}

static uint64_t alloc_stack(thread_t* thread) {
    void* stack_bottom = kmalloc(PROCESS_STACK_SIZE);
    if (stack_bottom == NULL) return 0;

    thread->stack_base = stack_bottom;

    uint64_t stack_top = (uint64_t)stack_bottom + PROCESS_STACK_SIZE;
    stack_top &= ~0xFULL;
    return stack_top;
}

static void reap_zombies_unsafe(void) {
    thread_t* prev = NULL;
    thread_t* node = zombie_queue_head;

    while (node != NULL) {
        if (thread_is_current_on_any_cpu(node)) {
            prev = node;
            node = node->next;
            continue;
        }

        thread_t* dead = node;
        node = node->next;

        if (prev == NULL) {
            zombie_queue_head = node;
        } else {
            prev->next = node;
        }

        if (dead->stack_base != NULL) {
            kfree(dead->stack_base);
        }
        kfree(dead);
    }
}

__attribute__((noreturn)) void thread_exit(void) {
    uint64_t irq_flags = spinlock_lock_irqsave(&scheduler_lock);

    thread_t* current_thread = current_threads[cpu_slot_index()];
    if (current_thread != NULL) {
        current_thread->status = THREAD_DEAD;
    }

    spinlock_unlock_irqrestore(&scheduler_lock, irq_flags);

    asm volatile ("int $0x20");

    for (;;) {
        asm volatile ("hlt");
    }
}

static __attribute__((noreturn)) void thread_entry_trampoline(void) {
    thread_t* thread = scheduler_current_thread();
    if (thread == NULL || thread->entry == NULL) {
        thread_exit();
    }

    thread->entry(thread->entry_arg);
    thread_exit();
}

cpu_status_t* schedule(cpu_status_t* context) {
    size_t cpu_index = cpu_slot_index();
    uint64_t irq_flags = spinlock_lock_irqsave(&scheduler_lock);

    thread_t* current_thread = current_threads[cpu_index];

    if (current_thread != NULL) {
        current_thread->context = context;

        if (current_thread->status == THREAD_RUNNING) {
            current_thread->status = THREAD_READY;

            if (current_thread != idle_thread) {
                enqueue_thread_unsafe(current_thread);
            }
        } else if (current_thread->status == THREAD_DEAD) {
            current_thread->next = zombie_queue_head;
            zombie_queue_head = current_thread;
        }
    }

    thread_t* next_thread = dequeue_thread_unsafe();

    if (next_thread == NULL) {
        next_thread = idle_thread;
    }

    if (next_thread == NULL) {
        spinlock_unlock_irqrestore(&scheduler_lock, irq_flags);
        return context;
    }

    current_threads[cpu_index] = next_thread;
    next_thread->status = THREAD_RUNNING;
    cpu_status_t* next_context = next_thread->context;

    reap_zombies_unsafe();

    spinlock_unlock_irqrestore(&scheduler_lock, irq_flags);

    phys_addr_t new_cr3 = 0;

    if (next_thread == idle_thread) {
        new_cr3 = vmm_get_phys(kernel_pml4);
    } else if (next_thread->parent != NULL && next_thread->parent->pml4 != NULL) {
        new_cr3 = vmm_get_phys(next_thread->parent->pml4);
    }

    if (new_cr3 != 0 && read_cr3() != new_cr3) {
        write_cr3(new_cr3);
    }

    return next_context;
}

static thread_t* create_thread(process_t* parent, void(*function)(void*), void* arg) {
    if (!parent || !function) return NULL;

    thread_t* thread = kmalloc(sizeof(thread_t));
    if (!thread) return NULL;

    memset(thread, 0, sizeof(thread_t));

    uint64_t stack_top = alloc_stack(thread);
    if (stack_top == 0) {
        kfree(thread);
        return NULL;
    }

    thread->status = THREAD_READY;
    thread->parent = parent;
    thread->entry = function;
    thread->entry_arg = arg;

    thread->context = (cpu_status_t*)(stack_top - sizeof(cpu_status_t));
    memset(thread->context, 0, sizeof(cpu_status_t));

    thread->context->iret_ss = KERNEL_DATA;
    thread->context->iret_rsp = stack_top;
    thread->context->iret_flags = 0x202;
    thread->context->iret_cs = KERNEL_CODE;
    thread->context->iret_rip = (uint64_t)thread_entry_trampoline;

    uint64_t irq_flags = spinlock_lock_irqsave(&scheduler_lock);

    thread->tid = next_tid++;

    thread->sibling = parent->threads;
    parent->threads = thread;

    enqueue_thread_unsafe(thread);

    spinlock_unlock_irqrestore(&scheduler_lock, irq_flags);

    return thread;
}

process_t* create_process(char* name, void(*function)(void*), void* arg) {
    if (!name || !function) return NULL;

    process_t* process = kmalloc(sizeof(process_t));
    if (!process) return NULL;

    memset(process, 0, sizeof(process_t));

    strncpy(process->name, name, PROC_NAME_LEN - 1);
    process->name[PROC_NAME_LEN - 1] = '\0';

    process->pml4 = vmm_create_new_pml4();
    if (process->pml4 == NULL) {
        kfree(process);
        return NULL;
    }

    uint64_t irq_flags = spinlock_lock_irqsave(&scheduler_lock);
    process->pid = next_pid++;
    process->next = processes_list;
    processes_list = process;
    spinlock_unlock_irqrestore(&scheduler_lock, irq_flags);

    thread_t* main_thread = create_thread(process, function, arg);
    if (main_thread == NULL) {
        irq_flags = spinlock_lock_irqsave(&scheduler_lock);
        if (processes_list == process) {
            processes_list = process->next;
        } else {
            process_t* iter = processes_list;
            while (iter && iter->next != process) {
                iter = iter->next;
            }
            if (iter != NULL) {
                iter->next = process->next;
            }
        }
        spinlock_unlock_irqrestore(&scheduler_lock, irq_flags);
        kfree(process);
        return NULL;
    }

    if (strcmp(process->name, "idle") == 0) {
        irq_flags = spinlock_lock_irqsave(&scheduler_lock);
        idle_thread = main_thread;
        spinlock_unlock_irqrestore(&scheduler_lock, irq_flags);
    }

    return process;
}