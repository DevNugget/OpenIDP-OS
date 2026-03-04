#ifndef SHM_H
#define SHM_H

#include <stdint.h>
#include <stddef.h>
#include <multitasking/process.h>

#define SHM_ACCESS_RW 0x1

typedef struct shm_mapping_t {
    uint64_t base;
    size_t page_count;
    uint64_t handle;
    struct shm_mapping_t* next;
} shm_mapping_t;

int shm_create(size_t size_bytes, uint64_t* out_handle);
int shm_create_from_phys(phys_addr_t phys_base, size_t size_bytes, uint64_t* out_handle);
int shm_map(process_t* process, uint64_t handle, uint64_t* out_addr);
int shm_unmap(process_t* process, uint64_t address);
int shm_destroy(uint64_t handle);
void shm_release_process_mappings(process_t* process);

#endif
