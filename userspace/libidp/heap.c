#include <libidp/heap.h>
#include <libidp/syscall.h>

#define HEAP_ALIGNMENT 16ULL
#define HEAP_PAGE_SIZE 4096ULL
#define HEAP_DEFAULT_SEGMENT_SIZE (64ULL * 1024ULL)
#define HEAP_BLOCK_MAGIC 0x48454150U /* HEAP */
#define HEAP_FLAG_FREE 0x1U

typedef struct heap_segment_t heap_segment_t;
typedef struct heap_block_t heap_block_t;

typedef struct heap_block_t {
    size_t size;
    size_t prev_size;
    uint32_t flags;
    uint32_t magic;
    heap_segment_t* segment;
    heap_block_t* prev_free;
    heap_block_t* next_free;
} heap_block_t;

typedef struct heap_segment_t {
    uint64_t shm_handle;
    size_t mapped_size;
    heap_segment_t* next;
    heap_segment_t* prev;
    heap_block_t* first_block;
} heap_segment_t;

typedef struct heap_state_t {
    volatile uint32_t lock;
    heap_segment_t* segments;
    heap_block_t* free_list;
    idp_heap_stats_t stats;
} heap_state_t;

static heap_state_t g_heap;

#define BLOCK_HEADER_SIZE ((sizeof(heap_block_t) + (HEAP_ALIGNMENT - 1ULL)) & ~(HEAP_ALIGNMENT - 1ULL))
#define MIN_SPLIT_SIZE (BLOCK_HEADER_SIZE + HEAP_ALIGNMENT)

static size_t align_up(size_t value, size_t alignment) {
    return (value + (alignment - 1ULL)) & ~(alignment - 1ULL);
}

static void heap_lock(void) {
    while (__sync_lock_test_and_set(&g_heap.lock, 1U) != 0U) {
        __asm__ volatile("pause");
    }
}

static void heap_unlock(void) {
    __sync_lock_release(&g_heap.lock);
}

static void memory_zero(void* dst, size_t bytes) {
    uint8_t* d = (uint8_t*)dst;
    for (size_t i = 0; i < bytes; ++i) {
        d[i] = 0;
    }
}

static int memory_copy(void* dst, const void* src, size_t bytes) {
    if (dst == NULL || src == NULL) {
        return -1;
    }

    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;

    if (d < s) {
        for (size_t i = 0; i < bytes; ++i) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (size_t i = bytes; i > 0; --i) {
            d[i - 1] = s[i - 1];
        }
    }

    return 0;
}

static heap_block_t* block_next(heap_block_t* block) {
    if (block == NULL || block->segment == NULL) {
        return NULL;
    }

    uint8_t* next_addr = (uint8_t*)block + block->size;
    uint8_t* seg_start = (uint8_t*)block->segment;
    uint8_t* seg_end = seg_start + block->segment->mapped_size;

    if (next_addr + BLOCK_HEADER_SIZE > seg_end) {
        return NULL;
    }

    return (heap_block_t*)next_addr;
}

static heap_block_t* block_prev(heap_block_t* block) {
    if (block == NULL || block->segment == NULL || block->prev_size == 0) {
        return NULL;
    }

    uint8_t* prev_addr = (uint8_t*)block - block->prev_size;
    uint8_t* seg_start = (uint8_t*)block->segment;

    if (prev_addr < seg_start || prev_addr + BLOCK_HEADER_SIZE > (uint8_t*)block) {
        return NULL;
    }

    return (heap_block_t*)prev_addr;
}

static void free_list_insert(heap_block_t* block) {
    block->flags |= HEAP_FLAG_FREE;
    block->prev_free = NULL;
    block->next_free = g_heap.free_list;
    if (g_heap.free_list != NULL) {
        g_heap.free_list->prev_free = block;
    }
    g_heap.free_list = block;
    g_heap.stats.free_block_count++;
}

static void free_list_remove(heap_block_t* block) {
    if ((block->flags & HEAP_FLAG_FREE) == 0) {
        return;
    }

    if (block->prev_free != NULL) {
        block->prev_free->next_free = block->next_free;
    } else {
        g_heap.free_list = block->next_free;
    }

    if (block->next_free != NULL) {
        block->next_free->prev_free = block->prev_free;
    }

    block->prev_free = NULL;
    block->next_free = NULL;
    block->flags &= ~HEAP_FLAG_FREE;

    if (g_heap.stats.free_block_count > 0) {
        g_heap.stats.free_block_count--;
    }
}

static void split_block_if_needed(heap_block_t* block, size_t needed_size) {
    if (block->size < needed_size + MIN_SPLIT_SIZE) {
        return;
    }

    size_t remainder_size = block->size - needed_size;
    block->size = needed_size;

    heap_block_t* remainder = (heap_block_t*)((uint8_t*)block + block->size);
    remainder->size = remainder_size;
    remainder->prev_size = block->size;
    remainder->flags = HEAP_FLAG_FREE;
    remainder->magic = HEAP_BLOCK_MAGIC;
    remainder->segment = block->segment;
    remainder->prev_free = NULL;
    remainder->next_free = NULL;

    heap_block_t* next = block_next(remainder);
    if (next != NULL) {
        next->prev_size = remainder->size;
    }

    free_list_insert(remainder);
}

static heap_block_t* coalesce_block(heap_block_t* block) {
    heap_block_t* next = block_next(block);
    if (next != NULL && next->magic == HEAP_BLOCK_MAGIC && (next->flags & HEAP_FLAG_FREE) != 0U) {
        free_list_remove(next);
        block->size += next->size;
        heap_block_t* next_after = block_next(block);
        if (next_after != NULL) {
            next_after->prev_size = block->size;
        }
    }

    heap_block_t* prev = block_prev(block);
    if (prev != NULL && prev->magic == HEAP_BLOCK_MAGIC && (prev->flags & HEAP_FLAG_FREE) != 0U) {
        free_list_remove(prev);
        prev->size += block->size;
        heap_block_t* next_after = block_next(prev);
        if (next_after != NULL) {
            next_after->prev_size = prev->size;
        }
        return prev;
    }

    return block;
}

static heap_block_t* find_suitable_block(size_t needed_size) {
    heap_block_t* best = NULL;
    for (heap_block_t* cur = g_heap.free_list; cur != NULL; cur = cur->next_free) {
        if (cur->magic != HEAP_BLOCK_MAGIC) {
            continue;
        }

        if (cur->size >= needed_size) {
            if (best == NULL || cur->size < best->size) {
                best = cur;
                if (cur->size == needed_size) {
                    break;
                }
            }
        }
    }
    return best;
}

static heap_block_t* create_segment(size_t min_needed) {
    size_t mapped = HEAP_DEFAULT_SEGMENT_SIZE;
    if (mapped < min_needed + sizeof(heap_segment_t) + BLOCK_HEADER_SIZE) {
        mapped = min_needed + sizeof(heap_segment_t) + BLOCK_HEADER_SIZE;
    }
    mapped = align_up(mapped, HEAP_PAGE_SIZE);

    int64_t handle = sys_shm_create(mapped);
    if (handle < 0) {
        g_heap.stats.failed_allocation_count++;
        return NULL;
    }

    void* mapped_base = sys_shm_map((uint64_t)handle);
    if (mapped_base == NULL) {
        sys_shm_destroy((uint64_t)handle);
        g_heap.stats.failed_allocation_count++;
        return NULL;
    }

    heap_segment_t* segment = (heap_segment_t*)mapped_base;
    memory_zero(segment, sizeof(*segment));
    segment->shm_handle = (uint64_t)handle;
    segment->mapped_size = mapped;
    segment->next = g_heap.segments;
    if (g_heap.segments != NULL) {
        g_heap.segments->prev = segment;
    }
    g_heap.segments = segment;
    g_heap.stats.segment_count++;
    g_heap.stats.total_mapped_bytes += mapped;

    uint8_t* block_start = (uint8_t*)segment + align_up(sizeof(heap_segment_t), HEAP_ALIGNMENT);
    size_t usable = mapped - (size_t)(block_start - (uint8_t*)segment);

    heap_block_t* first = (heap_block_t*)block_start;
    first->size = align_up(usable, HEAP_ALIGNMENT);
    first->prev_size = 0;
    first->flags = HEAP_FLAG_FREE;
    first->magic = HEAP_BLOCK_MAGIC;
    first->segment = segment;
    first->prev_free = NULL;
    first->next_free = NULL;

    segment->first_block = first;
    free_list_insert(first);

    return first;
}

static heap_block_t* user_ptr_to_block(void* ptr) {
    if (ptr == NULL) {
        return NULL;
    }
    return (heap_block_t*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
}

static void* block_to_user_ptr(heap_block_t* block) {
    return (void*)((uint8_t*)block + BLOCK_HEADER_SIZE);
}

void* malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size_t payload_size = align_up(size, HEAP_ALIGNMENT);
    size_t needed_size = payload_size + BLOCK_HEADER_SIZE;

    heap_lock();

    heap_block_t* block = find_suitable_block(needed_size);
    if (block == NULL) {
        block = create_segment(needed_size);
        if (block != NULL) {
            block = find_suitable_block(needed_size);
        }
    }

    if (block == NULL) {
        g_heap.stats.failed_allocation_count++;
        heap_unlock();
        return NULL;
    }

    free_list_remove(block);
    split_block_if_needed(block, needed_size);

    block->flags &= ~HEAP_FLAG_FREE;
    block->magic = HEAP_BLOCK_MAGIC;

    g_heap.stats.allocation_count++;
    g_heap.stats.current_used_bytes += payload_size;
    g_heap.stats.total_allocated_bytes += payload_size;
    if (g_heap.stats.current_used_bytes > g_heap.stats.peak_used_bytes) {
        g_heap.stats.peak_used_bytes = g_heap.stats.current_used_bytes;
    }

    void* result = block_to_user_ptr(block);
    heap_unlock();
    return result;
}

void free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    heap_lock();

    heap_block_t* block = user_ptr_to_block(ptr);
    if (block->magic != HEAP_BLOCK_MAGIC || (block->flags & HEAP_FLAG_FREE) != 0U) {
        heap_unlock();
        return;
    }

    size_t payload_size = block->size - BLOCK_HEADER_SIZE;
    if (g_heap.stats.current_used_bytes >= payload_size) {
        g_heap.stats.current_used_bytes -= payload_size;
    } else {
        g_heap.stats.current_used_bytes = 0;
    }
    g_heap.stats.total_freed_bytes += payload_size;
    g_heap.stats.free_count++;

    block->flags |= HEAP_FLAG_FREE;
    heap_block_t* merged = coalesce_block(block);
    free_list_insert(merged);

    heap_unlock();
}

void* calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) {
        return NULL;
    }

    if (count > ((size_t)-1) / size) {
        heap_lock();
        g_heap.stats.failed_allocation_count++;
        heap_unlock();
        return NULL;
    }

    size_t total = count * size;
    void* ptr = malloc(total);
    if (ptr != NULL) {
        memory_zero(ptr, total);
    }
    return ptr;
}

void* realloc(void* ptr, size_t new_size) {
    if (ptr == NULL) {
        return malloc(new_size);
    }
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    heap_lock();
    heap_block_t* block = user_ptr_to_block(ptr);
    if (block->magic != HEAP_BLOCK_MAGIC || (block->flags & HEAP_FLAG_FREE) != 0U) {
        heap_unlock();
        return NULL;
    }

    size_t old_payload = block->size - BLOCK_HEADER_SIZE;
    size_t wanted_payload = align_up(new_size, HEAP_ALIGNMENT);
    size_t wanted_total = wanted_payload + BLOCK_HEADER_SIZE;

    if (block->size >= wanted_total) {
        split_block_if_needed(block, wanted_total);
        if (wanted_payload > old_payload) {
            g_heap.stats.current_used_bytes += (wanted_payload - old_payload);
            g_heap.stats.total_allocated_bytes += (wanted_payload - old_payload);
        } else {
            g_heap.stats.current_used_bytes -= (old_payload - wanted_payload);
            g_heap.stats.total_freed_bytes += (old_payload - wanted_payload);
        }
        if (g_heap.stats.current_used_bytes > g_heap.stats.peak_used_bytes) {
            g_heap.stats.peak_used_bytes = g_heap.stats.current_used_bytes;
        }
        heap_unlock();
        return ptr;
    }

    heap_block_t* next = block_next(block);
    if (next != NULL && next->magic == HEAP_BLOCK_MAGIC && (next->flags & HEAP_FLAG_FREE) != 0U &&
        (block->size + next->size) >= wanted_total) {
        free_list_remove(next);
        block->size += next->size;
        heap_block_t* nn = block_next(block);
        if (nn != NULL) {
            nn->prev_size = block->size;
        }
        split_block_if_needed(block, wanted_total);

        g_heap.stats.current_used_bytes += (wanted_payload - old_payload);
        g_heap.stats.total_allocated_bytes += (wanted_payload - old_payload);
        if (g_heap.stats.current_used_bytes > g_heap.stats.peak_used_bytes) {
            g_heap.stats.peak_used_bytes = g_heap.stats.current_used_bytes;
        }

        heap_unlock();
        return ptr;
    }

    heap_unlock();

    void* new_ptr = malloc(new_size);
    if (new_ptr == NULL) {
        return NULL;
    }

    size_t copy_bytes = old_payload < new_size ? old_payload : new_size;
    memory_copy(new_ptr, ptr, copy_bytes);
    free(ptr);
    return new_ptr;
}

void* aligned_alloc(size_t alignment, size_t size) {
    if (alignment < sizeof(void*) || (alignment & (alignment - 1ULL)) != 0ULL) {
        return NULL;
    }

    if (alignment > HEAP_ALIGNMENT) {
        return NULL;
    }

    return malloc(size);
}

void idp_heap_get_stats(idp_heap_stats_t* out_stats) {
    if (out_stats == NULL) {
        return;
    }

    heap_lock();
    *out_stats = g_heap.stats;
    heap_unlock();
}

int idp_heap_validate(void) {
    heap_lock();

    for (heap_segment_t* seg = g_heap.segments; seg != NULL; seg = seg->next) {
        uint8_t* seg_start = (uint8_t*)seg;
        uint8_t* seg_end = seg_start + seg->mapped_size;

        heap_block_t* cur = seg->first_block;
        size_t walked = (size_t)((uint8_t*)cur - seg_start);

        while (cur != NULL) {
            if (cur->magic != HEAP_BLOCK_MAGIC || cur->segment != seg || cur->size < BLOCK_HEADER_SIZE) {
                heap_unlock();
                return -1;
            }

            walked += cur->size;
            if (seg_start + walked > seg_end) {
                heap_unlock();
                return -1;
            }

            heap_block_t* next = block_next(cur);
            if (next != NULL && next->prev_size != cur->size) {
                heap_unlock();
                return -1;
            }

            cur = next;
        }
    }

    heap_unlock();
    return 0;
}
