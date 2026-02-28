#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>

#include <multitasking/process.h>

int elf64_load_process_image(process_t* process, const void* image, size_t image_size, uint64_t* out_entry_point);

#endif //ELF_LOADER_H
