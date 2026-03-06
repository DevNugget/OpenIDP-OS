#ifndef LIBIDP_HEAP_H
#define LIBIDP_HEAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct idp_heap_stats_t {
    size_t total_mapped_bytes;
    size_t current_used_bytes;
    size_t peak_used_bytes;
    size_t total_allocated_bytes;
    size_t total_freed_bytes;
    size_t allocation_count;
    size_t free_count;
    size_t failed_allocation_count;
    size_t segment_count;
    size_t free_block_count;
} idp_heap_stats_t;

void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t count, size_t size);
void* realloc(void* ptr, size_t new_size);
void* aligned_alloc(size_t alignment, size_t size);

void idp_heap_get_stats(idp_heap_stats_t* out_stats);
int idp_heap_validate(void);

#ifdef __cplusplus
}
#endif

#endif
