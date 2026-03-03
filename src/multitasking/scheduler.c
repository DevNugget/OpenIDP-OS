#include <multitasking/scheduler.h>
#include <multitasking/process.h>
#include <multitasking/smp.h>

#include <descriptors/gdt.h>

#include <drivers/apic.h>
#include <drivers/com1.h>

#include <elf/elf_loader.h>
#include <fs/vfs.h>

#include <memory/kheap.h>
#include <memory/pmm.h>
#include <memory/vmm.h>

#include <utility/kstring.h>
#include <utility/spinlock.h>

#include <limine.h>

#define PROCESS_STACK_SIZE (128 * 1024)
#define DEFAULT_TIME_SLICE_TICKS 4
#define SIMD_STATE_SIZE 512
#define KERNEL_CONTEXT_SIZE (sizeof(cpu_status_t) - (2 * sizeof(uint64_t)))

#define USER_STACK_TOP 0x00007FFFFFFFE000ULL
#define USER_SHM_BASE  0x0000600000000000ULL
#define USER_STACK_PAGES 512
#define USER_STACK_GUARD_PAGES 1
#define USER_PROCESS_FILE_CHUNK 1024

#define VFS_O_READ 0x1

extern virt_addr_t* kernel_pml4;

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
static spinlock_t scheduler_init_lock = SPINLOCK_INIT;

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
    if (__atomic_load_n(&scheduler_initialized, __ATOMIC_ACQUIRE)) {
        return;
    }

    uint64_t init_flags = spinlock_lock_irqsave(&scheduler_init_lock);
    if (scheduler_initialized) {
        spinlock_unlock_irqrestore(&scheduler_init_lock, init_flags);
        return;
    }

    size_t cpu_count = smp_get_cpu_count();
    if (cpu_count == 0) {
        cpu_count = 1;
    }

    scheduler_cpu_count = cpu_count;

    current_threads = kmalloc(sizeof(thread_t*) * scheduler_cpu_count);
    idle_threads = kmalloc(sizeof(thread_t*) * scheduler_cpu_count);
    deferred_threads = kmalloc(sizeof(thread_t*) * scheduler_cpu_count);
    cpu_apic_ids = kmalloc(sizeof(uint32_t) * scheduler_cpu_count);

    if (current_threads == NULL || idle_threads == NULL || deferred_threads == NULL || cpu_apic_ids == NULL) {
        scheduler_cpu_count = 1;
        spinlock_unlock_irqrestore(&scheduler_init_lock, init_flags);
        return;
    }

    memset(current_threads, 0, sizeof(thread_t*) * scheduler_cpu_count);
    memset(idle_threads, 0, sizeof(thread_t*) * scheduler_cpu_count);
    memset(deferred_threads, 0, sizeof(thread_t*) * scheduler_cpu_count);
    memset(cpu_apic_ids, 0xFF, sizeof(uint32_t) * scheduler_cpu_count);

    __atomic_store_n(&scheduler_initialized, 1, __ATOMIC_RELEASE);
    spinlock_unlock_irqrestore(&scheduler_init_lock, init_flags);
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

static int allocate_user_stack(process_t* process, uint64_t* out_stack_top) {
    if (process == NULL || process->pml4 == NULL || out_stack_top == NULL) {
        return 0;
    }

    virt_addr_t stack_base = USER_STACK_TOP - ((USER_STACK_PAGES + USER_STACK_GUARD_PAGES) * PAGE_SIZE);
    for (size_t i = USER_STACK_GUARD_PAGES; i < USER_STACK_PAGES + USER_STACK_GUARD_PAGES; i++) {
        phys_addr_t phys = pmm_alloc(1);
        if (phys == 0) {
            return 0;
        }

        vmm_map_page((phys_addr_t*)process->pml4,
                     stack_base + (i * PAGE_SIZE),
                     phys,
                     PT_FLAG_USER | PT_FLAG_WRITE | PT_FLAG_NX);
    }

    *out_stack_top = USER_STACK_TOP;
    return 1;
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

    if (dead->parent != NULL) {
        uint64_t p_flags = spinlock_lock_irqsave(&process_lock);
        thread_t** t_prev = &dead->parent->threads;
        thread_t* t_cur = dead->parent->threads;
        while (t_cur) {
            if (t_cur == dead) {
                *t_prev = t_cur->sibling;
                break;
            }
            t_prev = &t_cur->sibling;
            t_cur = t_cur->sibling;
        }
        spinlock_unlock_irqrestore(&process_lock, p_flags);
    }

    if (dead->stack_base != NULL) kfree(dead->stack_base);
    if (dead->simd_state_alloc != NULL) kfree(dead->simd_state_alloc);
    kfree(dead);
    serial_printf("Reaped zombie.\n");

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
    thread_t* next_thread;
    
    while ((next_thread = dequeue_next_runnable_thread()) != NULL) {
        if (next_thread->status != THREAD_DEAD) {
            return next_thread;
        }
        queue_zombie(next_thread);
    }

    if (current_thread != NULL && current_thread->status != THREAD_DEAD) {
        current_thread->quantum_ticks = 0;
        return current_thread;
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

    if (current_thread != idle_threads[cpu_index]) {
        if (current_thread->status == THREAD_READY || current_thread->status == THREAD_DEAD) {
            deferred_threads[cpu_index] = current_thread;
        }
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

    if (next_thread->is_user_thread) {
        extern void gdt_set_tss_rsp0(size_t cpu_index, uint64_t rsp0);
        gdt_set_tss_rsp0(cpu_index, (uint64_t)next_thread->stack_base + PROCESS_STACK_SIZE);
    }

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

static void initialize_thread_context(thread_t* thread, uint64_t stack_top, int user_mode, uint64_t instruction_pointer) {
    thread->context = (cpu_status_t*)(stack_top - sizeof(cpu_status_t));
    memset(thread->context, 0, sizeof(cpu_status_t));

    thread->context->iret_rsp = stack_top;
    thread->context->iret_flags = 0x202;

    if (user_mode) {
        thread->context->iret_ss = USER_DATA | 0x3;
        thread->context->iret_cs = USER_CODE | 0x3;
        thread->context->iret_rip = instruction_pointer;
    } else {
        thread->context->iret_ss = KERNEL_DATA;
        thread->context->iret_cs = KERNEL_CODE;
        thread->context->iret_rip = (uint64_t)thread_entry_trampoline;
    }
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

    initialize_thread_context(thread, stack_top, 0, 0);
    link_thread_to_process(parent, thread);

    if (enqueue) {
        enqueue_thread(thread);
    }

    return thread;
}

static thread_t* create_user_thread(process_t* parent, uint64_t entry_point, int enqueue, int argc, char kernel_argv[16][64]) {
    if (!parent || parent->pml4 == NULL || entry_point == 0) return NULL;

    thread_t* thread = kmalloc(sizeof(thread_t));
    if (!thread) return NULL;
    memset(thread, 0, sizeof(thread_t));

    uint64_t user_stack_top = 0;
    if (!allocate_user_stack(parent, &user_stack_top)) { kfree(thread); return NULL; }

    uint64_t kernel_stack_top = alloc_stack(thread);
    if (kernel_stack_top == 0) { kfree(thread); return NULL; }

    if (!allocate_thread_simd_state(thread)) {
        kfree(thread->stack_base); kfree(thread); return NULL;
    }

    thread->status = THREAD_READY;
    thread->parent = parent;
    thread->is_user_thread = 1;

    uint64_t rsp = user_stack_top;
    uint64_t rflags;
    asm volatile("pushfq; pop %0; cli" : "=r"(rflags));
    
    phys_addr_t current_cr3 = read_cr3();
    write_cr3((vmm_get_phys(parent->pml4) & ~0xFFFULL) | (current_cr3 & 0xFFFULL));

    uint64_t user_argv_ptrs[16];

    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(kernel_argv[i]) + 1;
        rsp -= len;
        strcpy((char*)rsp, kernel_argv[i]);
        user_argv_ptrs[i] = rsp;
    }

    rsp &= ~0xFULL;

    rsp -= sizeof(uint64_t);
    *(uint64_t*)rsp = 0; 
    for (int i = argc - 1; i >= 0; i--) {
        rsp -= sizeof(uint64_t);
        *(uint64_t*)rsp = user_argv_ptrs[i];
    }
    
    uint64_t final_argv_ptr = rsp;

    write_cr3(current_cr3);
    asm volatile("push %0; popfq" :: "r"(rflags));

    uint64_t stack_offset = user_stack_top - rsp;
    uint64_t user_mode_rsp = USER_STACK_TOP - stack_offset;

    initialize_thread_context(thread, kernel_stack_top, 1, entry_point);
    
    thread->context->iret_rsp = user_mode_rsp;
    thread->context->rdi = argc;
    thread->context->rsi = user_mode_rsp;

    link_thread_to_process(parent, thread);
    if (enqueue) enqueue_thread(thread);

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

    process->parent_pid = 0;
    process->exited = 0;
    process->exit_code = 0;
    process->parent = NULL;
    process->first_child = NULL;
    process->next_sibling = NULL;

    strncpy(process->name, name, PROC_NAME_LEN - 1);
    process->name[PROC_NAME_LEN - 1] = '\0';

    process->pml4 = vmm_create_new_pml4();
    if (process->pml4 == NULL) {
        kfree(process);
        return NULL;
    }

    uint64_t flags = spinlock_lock_irqsave(&process_lock);
    process->pid = next_pid++;
    process->shm_next_base = USER_SHM_BASE;
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

static int read_vfs_file_all(const char* path, void** out_data, size_t* out_size) {
    if (path == NULL || out_data == NULL || out_size == NULL) {
        return -1;
    }

    vfs_file_t* file = NULL;
    if (vfs_open(path, VFS_O_READ, &file) != VFS_OK) {
        return -1;
    }

    size_t capacity = USER_PROCESS_FILE_CHUNK;
    size_t length = 0;
    uint8_t* buffer = kmalloc(capacity);
    if (buffer == NULL) {
        vfs_close(file);
        return -1;
    }

    uint8_t chunk[USER_PROCESS_FILE_CHUNK];

    while (1) {
        size_t rd = 0;
        vfs_status_t st = vfs_read(file, chunk, sizeof(chunk), &rd);
        if (st != VFS_OK) {
            vfs_close(file);
            kfree(buffer);
            return -1;
        }

        if (rd == 0) {
            break;
        }

        if (length + rd > capacity) {
            size_t new_capacity = capacity;
            while (length + rd > new_capacity) {
                new_capacity *= 2;
            }

            uint8_t* new_buffer = kmalloc(new_capacity);
            if (new_buffer == NULL) {
                vfs_close(file);
                kfree(buffer);
                return -1;
            }

            memcpy(new_buffer, buffer, length);
            kfree(buffer);
            buffer = new_buffer;
            capacity = new_capacity;
        }

        memcpy(buffer + length, chunk, rd);
        length += rd;
    }

    vfs_close(file);

    *out_data = buffer;
    *out_size = length;
    return 0;
}

process_t* create_user_process_from_elf(char* name, const void* elf_image, size_t elf_image_size, int argc, char kernel_argv[16][64]) {
    process_t* process = create_process(name, NULL, NULL);
    if (process == NULL) return NULL;

    uint64_t entry_point = 0;
    if (elf64_load_process_image(process, elf_image, elf_image_size, &entry_point) != 0) {
        destroy_failed_process(process); return NULL;
    }

    if (create_user_thread(process, entry_point, 1, argc, kernel_argv) == NULL) {
        destroy_failed_process(process); return NULL;
    }
    return process;
}

process_t* create_user_process_from_path(char* name, const char* path, int argc, char kernel_argv[16][64]) {
    void* elf_image = NULL;
    size_t elf_size = 0;

    serial_printf("[SCHED] Attempting to load user process: %s\n", path);

    if (read_vfs_file_all(path, &elf_image, &elf_size) != 0) {
        serial_printf("[SCHED] FATAL: Failed to read %s from VFS.\n", path);
        return NULL;
    }

    serial_printf("[SCHED] Read %u bytes. Parsing ELF...\n", (uint32_t)elf_size);

    process_t* process = create_user_process_from_elf(name, elf_image, elf_size, argc, kernel_argv);
    if (process == NULL) {
        serial_printf("[SCHED] FATAL: ELF parsing failed for %s.\n", path);
    } else {
        serial_printf("[SCHED] Process %s created successfully!\n", name);
    }
    kfree(elf_image);

    return process;
}

size_t scheduler_current_pid(void) {
    thread_t* t = scheduler_current_thread();
    if (t == NULL || t->parent == NULL) {
        return 0;
    }
    return t->parent->pid;
}

static process_t* process_find_unsafe(size_t pid) {
    process_t* p = processes_list;
    while (p != NULL) {
        if (p->pid == pid) {
            return p;
        }
        p = p->next;
    }
    return NULL;
}

int scheduler_spawn_process(const char* path, const char** user_argv, size_t parent_pid, size_t* out_pid) {
    if (path == NULL || out_pid == NULL) return -1;

    char kernel_argv[16][64];
    int argc = 0;

    if (user_argv != NULL) {
        for (int i = 0; i < 16; i++) {
            if (user_argv[i] == NULL) break;
            strncpy(kernel_argv[i], user_argv[i], 63);
            kernel_argv[i][63] = '\0';
            argc++;
        }
    } else {
        strncpy(kernel_argv[0], path, 63);
        kernel_argv[0][63] = '\0';
        argc = 1;
    }

    process_t* process = create_user_process_from_path("spawned", path, argc, kernel_argv);
    if (process == NULL) return -1;

    uint64_t flags = spinlock_lock_irqsave(&process_lock);
    process_t* parent = process_find_unsafe(parent_pid);
    if (parent != NULL) {
        process->parent = parent;
        process->parent_pid = parent->pid;
        process->next_sibling = parent->first_child;
        parent->first_child = process;
    }
    spinlock_unlock_irqrestore(&process_lock, flags);

    *out_pid = process->pid;
    return 0;
}

static void kill_threads_of_process(process_t* process) {
    for (thread_t* t = process->threads; t != NULL; t = t->sibling) {
        t->status = THREAD_DEAD;
    }
}

static void kill_process_recursive_unsafe(process_t* process, int exit_code) {
    for (process_t* child = process->first_child; child != NULL; child = child->next_sibling) {
        kill_process_recursive_unsafe(child, 137);
    }

    process->exited = 1;
    process->exit_code = exit_code;
    kill_threads_of_process(process);
}

int scheduler_kill_process_tree(size_t pid, int exit_code) {
    uint64_t flags = spinlock_lock_irqsave(&process_lock);
    process_t* process = process_find_unsafe(pid);
    if (process == NULL) {
        spinlock_unlock_irqrestore(&process_lock, flags);
        return -1;
    }

    kill_process_recursive_unsafe(process, exit_code);
    spinlock_unlock_irqrestore(&process_lock, flags);
    return 0;
}

int scheduler_wait_process(size_t waiter_pid, size_t target_pid, int* out_exit_code) {
    (void)waiter_pid;
    if (out_exit_code == NULL) {
        return -1;
    }

    uint64_t flags = spinlock_lock_irqsave(&process_lock);
    process_t* process = process_find_unsafe(target_pid);
    if (process == NULL) {
        spinlock_unlock_irqrestore(&process_lock, flags);
        return -1;
    }

    if (!process->exited) {
        spinlock_unlock_irqrestore(&process_lock, flags);
        return 1;
    }

    *out_exit_code = process->exit_code;
    spinlock_unlock_irqrestore(&process_lock, flags);
    return 0;
}

static void idle_thread_func(void* arg);
static void reaper_thread_func(void* arg);

volatile int scheduler_ready_flag = 0;

void scheduler_create_init_processes(void) {
    process_t* kernel_process = create_process("system", NULL, NULL);

    process_t* idle_process = create_process("idle", NULL, NULL); 
    for (size_t i = 0; i < scheduler_cpu_count; i++) {
        thread_t* idle = create_thread(idle_process, idle_thread_func, NULL, 0);
        idle_threads[i] = idle;
    }

    create_thread(kernel_process, reaper_thread_func, NULL, 1);

    //__atomic_store_n(&scheduler_ready_flag, 1, __ATOMIC_SEQ_CST);
}

static void idle_thread_func(void* arg) {
    serial_printf("[SCHED] Idle Process Started\n");
    while (1) {
        asm volatile ("hlt");
    }
}

static int reap_exited_processes(void) {
    process_t* dead_proc = NULL;

    uint64_t flags = spinlock_lock_irqsave(&process_lock);
    process_t** prev = &processes_list;
    process_t* cur = processes_list;

    while (cur) {
        if (cur->exited && cur->threads == NULL) {
            *prev = cur->next;
            dead_proc = cur;
            
            if (dead_proc->parent) {
                process_t** child_prev = &dead_proc->parent->first_child;
                process_t* child_cur = dead_proc->parent->first_child;
                while (child_cur) {
                    if (child_cur == dead_proc) {
                        *child_prev = child_cur->next_sibling;
                        break;
                    }
                    child_prev = &child_cur->next_sibling;
                    child_cur = child_cur->next_sibling;
                }
            }

            process_t* orphan = dead_proc->first_child;
            while (orphan) {
                orphan->parent = NULL;
                process_t* next = orphan->next_sibling;
                orphan->next_sibling = NULL;
                orphan = next;
            }
            
            break;
        }
        prev = &cur->next;
        cur = cur->next;
    }
    spinlock_unlock_irqrestore(&process_lock, flags);

    if (dead_proc) {
        if (dead_proc->pml4) {
            pmm_free(vmm_get_phys(dead_proc->pml4), 1);
        }
        kfree(dead_proc);
        return 1;
    }
    return 0;
}

static void reaper_thread_func(void* arg) {
    while (1) {
        int reaped_thread = reap_one_zombie(); 
        int reaped_process = reap_exited_processes();

        if (!reaped_thread && !reaped_process) {
            asm volatile ("int $0x20");
        }
    }
}