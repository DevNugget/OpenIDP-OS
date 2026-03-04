#include <memory/shm.h>

#include <memory/kheap.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <utility/kstring.h>
#include <utility/spinlock.h>

#define SHM_FLAG_DESTROYED (1ULL << 0)
#define SHM_FLAG_OWNS_PAGES (1ULL << 1)
#define SHM_MAX_SIZE_BYTES (64ULL * 1024 * 1024)
#define SHM_USER_LIMIT     0x00007FFF00000000ULL

#define ERR_SUCCESS 0
#define ERR_FAIL   -1

typedef struct shm_segment_t {
    uint64_t handle;
    size_t page_count;
    phys_addr_t* pages;
    size_t ref_count;
    uint64_t flags;
    struct shm_segment_t* next;
} shm_segment_t;

static shm_segment_t* shm_segments = NULL;
static uint64_t next_shm_handle = 1;
static spinlock_t shm_lock = SPINLOCK_INIT;

static shm_segment_t* find_segment_unsafe(uint64_t handle) {
    shm_segment_t* segment = shm_segments;
    while (segment != NULL) {
        if (segment->handle == handle) {
            return segment;
        }
        segment = segment->next;
    }
    return NULL;
}

static void release_segment_pages(shm_segment_t* segment) {
    if (segment == NULL) {
        return;
    }

    if (segment->pages != NULL) {
        if ((segment->flags & SHM_FLAG_OWNS_PAGES) != 0) {
            for (size_t i = 0; i < segment->page_count; ++i) {
                if (segment->pages[i] != 0) {
                    pmm_free(segment->pages[i], 1);
                }
            }
        }
        kfree(segment->pages);
    }

    kfree(segment);
}

static void try_collect_segment_unsafe(shm_segment_t* segment) {
    if (segment == NULL || segment->ref_count != 0 || ((segment->flags & SHM_FLAG_DESTROYED) == 0)) {
        return;
    }

    if (shm_segments == segment) {
        shm_segments = segment->next;
        release_segment_pages(segment);
        return;
    }

    shm_segment_t* it = shm_segments;
    while (it != NULL && it->next != segment) {
        it = it->next;
    }

    if (it != NULL) {
        it->next = segment->next;
        release_segment_pages(segment);
    }
}

static int create_segment_internal(phys_addr_t phys_base,
                                   size_t size_bytes,
                                   int allocate_pages,
                                   uint64_t* out_handle) {
    if (out_handle == NULL || size_bytes == 0 || size_bytes > SHM_MAX_SIZE_BYTES) {
        return ERR_FAIL;
    }

    size_t page_count = (size_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    if (page_count == 0) {
        return ERR_FAIL;
    }

    shm_segment_t* segment = kmalloc(sizeof(shm_segment_t));
    if (segment == NULL) {
        return ERR_FAIL;
    }
    memset(segment, 0, sizeof(shm_segment_t));

    segment->pages = kmalloc(page_count * sizeof(phys_addr_t));
    if (segment->pages == NULL) {
        kfree(segment);
        return ERR_FAIL;
    }
    memset(segment->pages, 0, page_count * sizeof(phys_addr_t));

    if (allocate_pages) {
        segment->flags |= SHM_FLAG_OWNS_PAGES;
        for (size_t i = 0; i < page_count; ++i) {
            phys_addr_t page = pmm_alloc(1);
            if (page == 0) {
                release_segment_pages(segment);
                return ERR_FAIL;
            }

            memset((void*)phys_to_virt(page), 0, PAGE_SIZE);
            segment->pages[i] = page;
        }
    } else {
        phys_addr_t aligned_base = phys_base & ~(phys_addr_t)(PAGE_SIZE - 1);
        for (size_t i = 0; i < page_count; ++i) {
            segment->pages[i] = aligned_base + (i * PAGE_SIZE);
        }
    }

    uint64_t flags = spinlock_lock_irqsave(&shm_lock);
    segment->handle = next_shm_handle++;
    segment->page_count = page_count;
    segment->next = shm_segments;
    shm_segments = segment;
    spinlock_unlock_irqrestore(&shm_lock, flags);

    *out_handle = segment->handle;
    return ERR_SUCCESS;
}

int shm_create(size_t size_bytes, uint64_t* out_handle) {
    return create_segment_internal(0, size_bytes, 1, out_handle);
}

int shm_create_from_phys(phys_addr_t phys_base, size_t size_bytes, uint64_t* out_handle) {
    if (phys_base == 0) {
        return ERR_FAIL;
    }

    return create_segment_internal(phys_base, size_bytes, 0, out_handle);
}

int shm_map(process_t* process, uint64_t handle, uint64_t* out_addr) {
    if (process == NULL || process->pml4 == NULL || out_addr == NULL || handle == 0) {
        return ERR_FAIL;
    }

    uint64_t flags = spinlock_lock_irqsave(&shm_lock);
    shm_segment_t* segment = find_segment_unsafe(handle);
    if (segment == NULL || (segment->flags & SHM_FLAG_DESTROYED) != 0) {
        spinlock_unlock_irqrestore(&shm_lock, flags);
        return ERR_FAIL;
    }

    uint64_t size_bytes = segment->page_count * PAGE_SIZE;
    uint64_t base = (process->shm_next_base + (PAGE_SIZE - 1)) & ~(uint64_t)(PAGE_SIZE - 1);
    if (base + size_bytes >= SHM_USER_LIMIT) {
        spinlock_unlock_irqrestore(&shm_lock, flags);
        return ERR_FAIL;
    }

    shm_mapping_t* mapping = kmalloc(sizeof(shm_mapping_t));
    if (mapping == NULL) {
        spinlock_unlock_irqrestore(&shm_lock, flags);
        return ERR_FAIL;
    }

    for (size_t i = 0; i < segment->page_count; ++i) {
        vmm_map_page((phys_addr_t*)process->pml4,
                     base + (i * PAGE_SIZE),
                     segment->pages[i],
                     PT_FLAG_USER | PT_FLAG_WRITE | PT_FLAG_NX);
    }

    memset(mapping, 0, sizeof(shm_mapping_t));
    mapping->base = base;
    mapping->page_count = segment->page_count;
    mapping->handle = handle;
    mapping->next = process->shm_mappings;
    process->shm_mappings = mapping;

    process->shm_next_base = base + size_bytes;
    segment->ref_count++;

    write_cr3(read_cr3());
    spinlock_unlock_irqrestore(&shm_lock, flags);

    *out_addr = base;
    return ERR_SUCCESS;
}

int shm_unmap(process_t* process, uint64_t address) {
    if (process == NULL || process->pml4 == NULL || address == 0) {
        return ERR_FAIL;
    }

    uint64_t flags = spinlock_lock_irqsave(&shm_lock);

    shm_mapping_t** cursor = &process->shm_mappings;
    shm_mapping_t* mapping = process->shm_mappings;

    while (mapping != NULL && mapping->base != address) {
        cursor = &mapping->next;
        mapping = mapping->next;
    }

    if (mapping == NULL) {
        spinlock_unlock_irqrestore(&shm_lock, flags);
        return ERR_FAIL;
    }

    for (size_t i = 0; i < mapping->page_count; ++i) {
        vmm_unmap_page((phys_addr_t*)process->pml4, mapping->base + (i * PAGE_SIZE));
    }

    *cursor = mapping->next;

    shm_segment_t* segment = find_segment_unsafe(mapping->handle);
    if (segment != NULL && segment->ref_count > 0) {
        segment->ref_count--;
        try_collect_segment_unsafe(segment);
    }

    kfree(mapping);
    spinlock_unlock_irqrestore(&shm_lock, flags);

    return ERR_SUCCESS;
}

int shm_destroy(uint64_t handle) {
    if (handle == 0) {
        return ERR_FAIL;
    }

    uint64_t flags = spinlock_lock_irqsave(&shm_lock);
    shm_segment_t* segment = find_segment_unsafe(handle);
    if (segment == NULL) {
        spinlock_unlock_irqrestore(&shm_lock, flags);
        return ERR_FAIL;
    }

    segment->flags |= SHM_FLAG_DESTROYED;
    try_collect_segment_unsafe(segment);

    spinlock_unlock_irqrestore(&shm_lock, flags);
    return ERR_SUCCESS;
}

void shm_release_process_mappings(process_t* process) {
    if (process == NULL || process->pml4 == NULL) {
        return;
    }

    uint64_t flags = spinlock_lock_irqsave(&shm_lock);

    shm_mapping_t* mapping = process->shm_mappings;
    process->shm_mappings = NULL;

    while (mapping != NULL) {
        shm_mapping_t* next = mapping->next;

        for (size_t i = 0; i < mapping->page_count; ++i) {
            vmm_unmap_page((phys_addr_t*)process->pml4, mapping->base + (i * PAGE_SIZE));
        }

        shm_segment_t* segment = find_segment_unsafe(mapping->handle);
        if (segment != NULL && segment->ref_count > 0) {
            segment->ref_count--;
            try_collect_segment_unsafe(segment);
        }

        kfree(mapping);
        mapping = next;
    }

    spinlock_unlock_irqrestore(&shm_lock, flags);
}