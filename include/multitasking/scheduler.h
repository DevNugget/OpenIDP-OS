/* date = February 23rd 2026 4:53 pm */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include <utility/cpu_state.h>
#include <multitasking/process.h>

cpu_status_t* schedule(cpu_status_t* context);
process_t* create_process(char* name, void(*function)(void*), void* arg);
process_t* create_user_process_from_elf(char* name, const void* elf_image, size_t elf_image_size, int argc, char kernel_argv[16][64]);
process_t* create_user_process_from_path(char* name, const char* path, int argc, char kernel_argv[16][64]);

thread_t* scheduler_current_thread(void);
__attribute__((noreturn)) void thread_exit(void);
void scheduler_create_init_processes(void);
size_t scheduler_current_pid(void);
int scheduler_spawn_process(const char* path, const char** user_argv, size_t parent_pid, size_t* out_pid);
int scheduler_kill_process_tree(size_t pid, int exit_code);
int scheduler_wait_process(size_t waiter_pid, size_t target_pid, int* out_exit_code);

size_t scheduler_get_process_count(void);

typedef struct process_snapshot_entry_t {
    uint64_t pid;
    uint64_t parent_pid;
    uint32_t exited;
    uint32_t thread_count;
    uint32_t running_thread_count;
    uint32_t running_tid_count;
    uint64_t cpu_mask;
    uint64_t running_tids[8];
    char name[PROC_NAME_LEN];
} process_snapshot_entry_t;

size_t scheduler_copy_process_snapshot(process_snapshot_entry_t* buffer, size_t capacity);

#endif
