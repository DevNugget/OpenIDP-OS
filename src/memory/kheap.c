#include <kheap.h>

typedef struct heap_block {
    uint64_t magic;
    size_t size;
    int free;
    struct heap_block* next;
    struct heap_block* prev;
} heap_block_t;

static void heap_expand(size_t size);
static void split_block(heap_block_t* block, size_t size);
static void coalesce(heap_block_t* block);
static heap_block_t* get_last_block(void);

extern uint64_t* kernel_pml4;
extern uint64_t limine_hhdm;

static heap_block_t* heap_head = NULL;
static uint8_t* heap_end = (uint8_t*)KHEAP_START;

void heap_init(void) {
    // Start with 16KB heap
    heap_expand(PAGE_SIZE * 4);
}

void* kmalloc(size_t size) {
    if (!size) return NULL;

    size = ALIGN_UP(size, ALIGNMENT);

    heap_block_t* curr = heap_head;
    while (curr) {
        if (curr->free && curr->size >= size) {
            split_block(curr, size);
            curr->free = 0;
            return (void*)(curr + 1);
        }
        curr = curr->next;
    }

    // No block found → expand heap
    size_t total = size + sizeof(heap_block_t);
    heap_expand(total);

    heap_block_t* block = get_last_block();
    block->free = 0;
    return (void*)(block + 1) + limine_hhdm;
}

void kfree(void* ptr) {
    if (!ptr) return;

    heap_block_t* block = ((heap_block_t*)ptr) - 1;

    if (block->magic != HEAP_MAGIC) {
        // Optional: panic("Heap corruption detected");
        return;
    }

    block->free = 1;
    coalesce(block);
}

void* krealloc(void* ptr, size_t size) {
    if (!ptr) return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    heap_block_t* block = ((heap_block_t*)ptr) - 1;

    if (block->size >= size) return ptr;

    void* newptr = kmalloc(size);
    if (!newptr) return NULL;

    uint8_t* dst = newptr;
    uint8_t* src = ptr;
    for (size_t i = 0; i < block->size; i++)
        dst[i] = src[i];

    kfree(ptr);
    return newptr;
}

/*
Internal helper functions
*/

static void heap_expand(size_t size) {
    size = ALIGN_UP(size, PAGE_SIZE);

    for (size_t i = 0; i < size; i += PAGE_SIZE) {
        uint64_t phys = (uint64_t)pmm_alloc_page() - limine_hhdm;
        vmm_map_page(kernel_pml4,
                     (uint64_t)heap_end,
                     phys,
                     0x3); // PRESENT | WRITE
        heap_end += PAGE_SIZE;
    }

    heap_block_t* block = (heap_block_t*)(heap_end - size);
    block->magic = HEAP_MAGIC;
    block->size = size - sizeof(heap_block_t);
    block->free = 1;
    block->next = NULL;
    block->prev = get_last_block();

    if (block->prev)
        block->prev->next = block;
    else
        heap_head = block;
}

static heap_block_t* get_last_block(void) {
    heap_block_t* curr = heap_head;
    if (!curr) return NULL;
    while (curr->next) curr = curr->next;
    return curr;
}

static void split_block(heap_block_t* block, size_t size) {
    if (block->size < size + sizeof(heap_block_t) + MIN_SPLIT)
        return;

    heap_block_t* newblock = (heap_block_t*)((uint8_t*)(block + 1) + size);

    newblock->magic = HEAP_MAGIC;
    newblock->size = block->size - size - sizeof(heap_block_t);
    newblock->free = 1;
    newblock->next = block->next;
    newblock->prev = block;

    if (newblock->next)
        newblock->next->prev = newblock;

    block->next = newblock;
    block->size = size;
}

static void coalesce(heap_block_t* block) {
    // Merge forward
    if (block->next && block->next->free) {
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next)
            block->next->prev = block;
    }

    // Merge backward
    if (block->prev && block->prev->free) {
        coalesce(block->prev);
    }
}