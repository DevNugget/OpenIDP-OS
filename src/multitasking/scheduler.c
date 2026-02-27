#include <multitasking/scheduler.h>
#include <multitasking/process.h>

#include <descriptors/gdt.h>

#include <drivers/apic.h>
#include <drivers/com1.h>

#include <memory/kheap.h>
#include <memory/pmm.h>
#include <memory/vmm.h>

#include <utility/kstring.h>
#include <utility/spinlock.h>

#include <limine.h>

#define PROCESS_STACK_SIZE (64 * 1024)
#define DEFAULT_TIME_SLICE_TICKS 4
#define SIMD_STATE_SIZE 512

extern virt_addr_t* kernel_pml4;

__attribute__((used, section(".limine_requests")))
static volatile struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0
};

static thread_t* run_queue_head = NULL;
static thread_t* run_queue_tail = NULL;
static thread_t* zombie_queue_head = NULL;

static thread_t** current_threads = NULL;
static thread_t** idle_threads = NULL;
static thread_t** deferred_threads = NULL;
static uint32_t* cpu_apic_ids = NULL;
static size_t cpu_slots_used = 0;
static size_t scheduler_cpu_count = 1;

static process_t* processes_list = NULL;

static size_t next_tid = 1;
static size_t next_pid = 1;

static spinlock_t run_queue_lock = SPINLOCK_INIT;
static spinlock_t zombie_lock = SPINLOCK_INIT;
static spinlock_t cpu_map_lock = SPINLOCK_INIT;
static spinlock_t process_lock = SPINLOCK_INIT;

static int scheduler_initialized = 0;

static inline void save_simd_state(thread_t* thread) {
    if (thread == NULL || thread->simd_state == NULL) {
        return;
    }

    asm volatile("fxsave64 (%0)" :: "r"(thread->simd_state) : "memory");
    thread->simd_state_valid = 1;
}

static inline void restore_or_init_simd_state(thread_t* thread) {
    if (thread == NULL || thread->simd_state == NULL) {
        return;
    }

    if (thread->simd_state_valid) {
        asm volatile("fxrstor64 (%0)" :: "r"(thread->simd_state) : "memory");
    } else {
        asm volatile("fninit" ::: "memory");
        asm volatile("fxsave64 (%0)" :: "r"(thread->simd_state) : "memory");
        thread->simd_state_valid = 1;
    }
}

static void scheduler_init_once(void) {
    if (scheduler_initialized) {
        return;
    }

    size_t cpu_count = 1;
    if (mp_request.response != NULL && mp_request.response->cpu_count > 0) {
        cpu_count = (size_t)mp_request.response->cpu_count;
    }

    scheduler_cpu_count = cpu_count;

    current_threads = kmalloc(sizeof(thread_t*) * scheduler_cpu_count);
    idle_threads = kmalloc(sizeof(thread_t*) * scheduler_cpu_count);
    deferred_threads = kmalloc(sizeof(thread_t*) * scheduler_cpu_count);
    cpu_apic_ids = kmalloc(sizeof(uint32_t) * scheduler_cpu_count);

    if (current_threads == NULL || idle_threads == NULL || deferred_threads == NULL || cpu_apic_ids == NULL) {
        scheduler_cpu_count = 1;
        return;
    }

    memset(current_threads, 0, sizeof(thread_t*) * scheduler_cpu_count);
    memset(idle_threads, 0, sizeof(thread_t*) * scheduler_cpu_count);
    memset(deferred_threads, 0, sizeof(thread_t*) * scheduler_cpu_count);
    memset(cpu_apic_ids, 0xFF, sizeof(uint32_t) * scheduler_cpu_count);

    scheduler_initialized = 1;
}

static inline size_t cpu_slot_index(void) {
    scheduler_init_once();

    if (!scheduler_initialized) {
        return 0;
    }

    uint32_t apic_id = apic_get_id();
    uint64_t flags = spinlock_lock_irqsave(&cpu_map_lock);

    for (size_t i = 0; i < cpu_slots_used; ++i) {
        if (cpu_apic_ids[i] == apic_id) {
            spinlock_unlock_irqrestore(&cpu_map_lock, flags);
            return i;
        }
    }

    if (cpu_slots_used < scheduler_cpu_count) {
        cpu_apic_ids[cpu_slots_used] = apic_id;
        cpu_slots_used++;
        spinlock_unlock_irqrestore(&cpu_map_lock, flags);
        return cpu_slots_used - 1;
    }

    spinlock_unlock_irqrestore(&cpu_map_lock, flags);
    return 0;
}

thread_t* scheduler_current_thread(void) {
    size_t slot = cpu_slot_index();
    if (!scheduler_initialized || slot >= scheduler_cpu_count) {
        return NULL;
    }

    return current_threads[slot];
}

static int thread_is_current_on_any_cpu(const thread_t* thread) {
    if (!scheduler_initialized || !thread) {
        return 0;
    }

    for (size_t i = 0; i < scheduler_cpu_count; ++i) {
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
    uint8_t* stack_bottom = kmalloc(PROCESS_STACK_SIZE);
    if (stack_bottom == NULL) {
        return 0;
    }

    thread->stack_base = stack_bottom;

    uint64_t stack_top = (uint64_t)stack_bottom + PROCESS_STACK_SIZE;
    stack_top &= ~0xFULL;
    return stack_top;
}

static void queue_zombie(thread_t* thread) {
    if (!thread) {
        return;
    }

    uint64_t flags = spinlock_lock_irqsave(&zombie_lock);
    thread->next = zombie_queue_head;
    zombie_queue_head = thread;
    spinlock_unlock_irqrestore(&zombie_lock, flags);
}

static int reap_one_zombie(void) {
    thread_t* dead = NULL;

    uint64_t flags = spinlock_lock_irqsave(&zombie_lock);
    
    thread_t** prev = &zombie_queue_head;
    thread_t* cur = zombie_queue_head;

    while (cur) {
        if (!thread_is_current_on_any_cpu(cur)) {
            *prev = cur->next;
            dead = cur;
            break;
        }
        prev = &cur->next;
        cur = cur->next;
    }
    
    spinlock_unlock_irqrestore(&zombie_lock, flags);

    if (dead == NULL) {
        return 0;
    }

    if (dead->stack_base != NULL) kfree(dead->stack_base);
    if (dead->simd_state_alloc != NULL) kfree(dead->simd_state_alloc);
    kfree(dead);

    return 1;
}

__attribute__((noreturn)) void thread_exit(void) {
    thread_t* current_thread = scheduler_current_thread();
    if (current_thread != NULL) {
        current_thread->status = THREAD_DEAD;
    }

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

static void flush_deferred_thread(size_t cpu_index) {
    thread_t* deferred = deferred_threads[cpu_index];
    if (deferred == NULL) {
        return;
    }

    deferred_threads[cpu_index] = NULL;

    if (deferred->status == THREAD_READY) {
        uint64_t rq_flags = spinlock_lock_irqsave(&run_queue_lock);
        enqueue_thread_unsafe(deferred);
        spinlock_unlock_irqrestore(&run_queue_lock, rq_flags);
    } else if (deferred->status == THREAD_DEAD) {
        queue_zombie(deferred);
    }
}

static int should_switch_thread(thread_t* current_thread) {
    if (current_thread == NULL || current_thread->status != THREAD_RUNNING) {
        return 1;
    }

    current_thread->quantum_ticks++;
    return current_thread->quantum_ticks >= DEFAULT_TIME_SLICE_TICKS;
}

static thread_t* dequeue_next_runnable_thread(void) {
    uint64_t rq_flags = spinlock_lock_irqsave(&run_queue_lock);
    thread_t* next_thread = dequeue_thread_unsafe();
    spinlock_unlock_irqrestore(&run_queue_lock, rq_flags);
    return next_thread;
}

static thread_t* pick_next_thread(size_t cpu_index, thread_t* current_thread) {
    thread_t* next_thread = dequeue_next_runnable_thread();
    if (next_thread != NULL) {
        return next_thread;
    }

    next_thread = idle_threads[cpu_index];
    if (next_thread == NULL && current_thread != NULL && current_thread->status != THREAD_DEAD) {
        current_thread->quantum_ticks = 0;
        return current_thread;
    }

    return next_thread;
}

static void defer_current_if_needed(size_t cpu_index, thread_t* current_thread, thread_t* next_thread) {
    if (current_thread == NULL || current_thread == next_thread) {
        return;
    }

    if (current_thread->status == THREAD_RUNNING) {
        current_thread->status = THREAD_READY;
    }

    if (current_thread->status == THREAD_READY || current_thread->status == THREAD_DEAD) {
        deferred_threads[cpu_index] = current_thread;
    }

    save_simd_state(current_thread);
}

static void switch_address_space(size_t cpu_index, thread_t* next_thread) {
    phys_addr_t new_cr3 = 0;

    if (next_thread == idle_threads[cpu_index]) {
        new_cr3 = vmm_get_phys(kernel_pml4);
    } else if (next_thread->parent != NULL && next_thread->parent->pml4 != NULL) {
        new_cr3 = vmm_get_phys(next_thread->parent->pml4);
    }

    if (new_cr3 != 0 && read_cr3() != new_cr3) {
        write_cr3(new_cr3);
    }
}

cpu_status_t* schedule(cpu_status_t* context) {
    size_t cpu_index = cpu_slot_index();
    if (!scheduler_initialized || cpu_index >= scheduler_cpu_count) {
        return context;
    }

    thread_t* current_thread = current_threads[cpu_index];

    flush_deferred_thread(cpu_index);

    if (current_thread != NULL) {
        current_thread->context = context;
    }

    if (!should_switch_thread(current_thread)) {
        return context;
    }

    thread_t* next_thread = pick_next_thread(cpu_index, current_thread);

    if (next_thread == NULL) {
        return context;
    }

    if (next_thread == current_thread) {
        return context;
    }

    defer_current_if_needed(cpu_index, current_thread, next_thread);

    current_threads[cpu_index] = next_thread;
    next_thread->status = THREAD_RUNNING;
    next_thread->quantum_ticks = 0;

    restore_or_init_simd_state(next_thread);

    cpu_status_t* next_context = next_thread->context;
    switch_address_space(cpu_index, next_thread);

    return next_context;
}

static int allocate_thread_simd_state(thread_t* thread) {
    thread->simd_state_alloc = kmalloc(SIMD_STATE_SIZE + 16);
    if (thread->simd_state_alloc == NULL) {
        return 0;
    }

    uintptr_t aligned_simd = ((uintptr_t)thread->simd_state_alloc + 15U) & ~(uintptr_t)0xFU;
    thread->simd_state = (uint8_t*)aligned_simd;
    thread->simd_state_valid = 0;
    return 1;
}

static void initialize_thread_context(thread_t* thread, uint64_t stack_top) {
    thread->context = (cpu_status_t*)(stack_top - sizeof(cpu_status_t));
    memset(thread->context, 0, sizeof(cpu_status_t));

    thread->context->iret_ss = KERNEL_DATA;
    thread->context->iret_rsp = stack_top;
    thread->context->iret_flags = 0x202;
    thread->context->iret_cs = KERNEL_CODE;
    thread->context->iret_rip = (uint64_t)thread_entry_trampoline;
}

static void link_thread_to_process(process_t* parent, thread_t* thread) {
    uint64_t flags = spinlock_lock_irqsave(&process_lock);
    thread->tid = next_tid++;

    thread->sibling = parent->threads;
    parent->threads = thread;
    spinlock_unlock_irqrestore(&process_lock, flags);
}

static void enqueue_thread(thread_t* thread) {
    uint64_t rq_flags = spinlock_lock_irqsave(&run_queue_lock);
    enqueue_thread_unsafe(thread);
    spinlock_unlock_irqrestore(&run_queue_lock, rq_flags);
}

static thread_t* create_thread(process_t* parent, void(*function)(void*), void* arg, int enqueue) {
    if (!parent || !function) return NULL;

    thread_t* thread = kmalloc(sizeof(thread_t));
    if (!thread) return NULL;

    memset(thread, 0, sizeof(thread_t));

    uint64_t stack_top = alloc_stack(thread);
    if (stack_top == 0) {
        kfree(thread);
        return NULL;
    }

    if (!allocate_thread_simd_state(thread)) {
        kfree(thread->stack_base);
        kfree(thread);
        return NULL;
    }

    thread->status = THREAD_READY;
    thread->parent = parent;
    thread->entry = function;
    thread->entry_arg = arg;

    initialize_thread_context(thread, stack_top);
    link_thread_to_process(parent, thread);

    if (enqueue) {
        enqueue_thread(thread);
    }

    return thread;
}

static void unlink_process_unsafe(process_t* process) {
    if (processes_list == process) {
        processes_list = process->next;
        return;
    }

    process_t* iter = processes_list;
    while (iter && iter->next != process) {
        iter = iter->next;
    }

    if (iter != NULL) {
        iter->next = process->next;
    }
}

static void destroy_failed_process(process_t* process) {
    uint64_t flags = spinlock_lock_irqsave(&process_lock);
    unlink_process_unsafe(process);
    spinlock_unlock_irqrestore(&process_lock, flags);

    pmm_free(vmm_get_phys(process->pml4), 1);
    kfree(process);
}

process_t* create_process(char* name, void(*function)(void*), void* arg) {
    if (!name) return NULL;

    scheduler_init_once();

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

    uint64_t flags = spinlock_lock_irqsave(&process_lock);
    process->pid = next_pid++;
    process->next = processes_list;
    processes_list = process;
    spinlock_unlock_irqrestore(&process_lock, flags);

    if (function != NULL) {
        thread_t* main_thread = create_thread(process, function, arg, 1);
        if (main_thread == NULL) {
            destroy_failed_process(process);
            return NULL;
        }
    }

    return process;
}

static void idle_thread_func(void* arg);
static void reaper_thread_func(void* arg);

void scheduler_create_init_processes(void) {
    process_t* kernel_process = create_process("system", NULL, NULL);

    process_t* idle_process = create_process("idle", NULL, NULL); 
    for (size_t i = 0; i < scheduler_cpu_count; i++) {
        thread_t* idle = create_thread(idle_process, idle_thread_func, NULL, 0);
        idle_threads[i] = idle;
    }

    create_thread(kernel_process, reaper_thread_func, NULL, 1);
}

static void idle_thread_func(void* arg) {
    serial_printf("[SCHED] Idle Process Started\n");
    while (1) {
        asm volatile ("hlt");
    }
}

static void reaper_thread_func(void* arg) {
    while (1) {

        int reaped = reap_one_zombie(); 

        if (!reaped) {
            asm volatile ("int $0x20");
        }
    }
}