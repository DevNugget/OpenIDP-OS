/* date = February 23rd 2026 4:53 pm */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include <utility/cpu_state.h>
#include <multitasking/process.h>

cpu_status_t* schedule(cpu_status_t* context);
process_t* create_process(char* name, void(*function)(void*), void* arg);
process_t* create_user_process_from_elf(char* name, const void* elf_image, size_t elf_image_size);
process_t* create_user_process_from_path(char* name, const char* path);

thread_t* scheduler_current_thread(void);
__attribute__((noreturn)) void thread_exit(void);
void scheduler_create_init_processes(void);

#endif
