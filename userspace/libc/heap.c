#include "heap.h"
#include "../openidp.h"

typedef struct heap_block {
    uint64_t magic;
    uint64_t size;
    int free;
    struct heap_block* next;
    struct heap_block* prev;
} heap_block_t;

#define HEAP_MAGIC 0xCAFEBABE
#define MIN_SPLIT 32

static heap_block_t* heap_head = (void*)0;

static void split_block(heap_block_t* block, uint64_t size) {
    if (block->size < size + sizeof(heap_block_t) + MIN_SPLIT)
        return;

    heap_block_t* newblock = (heap_block_t*)((char*)(block + 1) + size);

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
    if (block->next && block->next->free) {
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }
    if (block->prev && block->prev->free) {
        coalesce(block->prev);
    }
}

void* malloc(uint64_t size) {
    if (size == 0) return (void*)0;
    
    // 16-byte alignment
    size = (size + 15) & ~15;

    // First time init
    if (!heap_head) {
        heap_head = (heap_block_t*)sbrk(0);
    }

    heap_block_t* curr = heap_head;
    heap_block_t* last = (void*)0;

    // Search for free block
    while (curr && (uint64_t)curr < (uint64_t)sbrk(0)) {
        if (curr->magic != HEAP_MAGIC) {
            // Heap corruption or uninitialized space (if sbrk moved but we didn't write)
            // For now, assume end of valid chain
            break; 
        }
        
        if (curr->free && curr->size >= size) {
            split_block(curr, size);
            curr->free = 0;
            return (void*)(curr + 1);
        }
        last = curr;
        curr = curr->next;
    }

    // No block found, request from OS
    uint64_t total_req = size + sizeof(heap_block_t);
    heap_block_t* block = (heap_block_t*)sbrk(total_req);
    
    if (block == (void*)-1) return (void*)0; // OOM

    block->magic = HEAP_MAGIC;
    block->size = size;
    block->free = 0;
    block->next = (void*)0;
    block->prev = last;

    if (last) {
        last->next = block;
    } else {
        heap_head = block;
    }

    return (void*)(block + 1);
}

void free(void* ptr) {
    if (!ptr) return;
    heap_block_t* block = ((heap_block_t*)ptr) - 1;
    if (block->magic != HEAP_MAGIC) return;
    block->free = 1;
    coalesce(block);
}