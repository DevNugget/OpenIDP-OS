/* date = February 23rd 2026 4:49 pm */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <utility/cpu_state.h>
#include <utility/hhdm.h>

#define PROC_NAME_LEN 64

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_DEAD
} thread_status_t;

struct thread_t;
struct shm_mapping_t;

typedef struct process_t {
    size_t pid;
    size_t parent_pid;
    char name[PROC_NAME_LEN];
    int exited;
    int exit_code;
    virt_addr_t* pml4;
    struct thread_t* threads;
    struct shm_mapping_t* shm_mappings;
    virt_addr_t shm_next_base;
    struct process_t* parent;
    struct process_t* first_child;
    struct process_t* next_sibling;
    struct process_t* next;
} process_t;

typedef struct thread_t {
    size_t tid;
    thread_status_t status;
    cpu_status_t* context;
    void* stack_base;
    void (*entry)(void*);
    void* entry_arg;
    uint8_t* simd_state_alloc;
    uint8_t* simd_state;
    uint8_t simd_state_valid;
    uint32_t quantum_ticks;
    uint8_t is_user_thread;
    process_t* parent;
    struct thread_t* next;
    struct thread_t* sibling;
} thread_t;

#endif
