#include <memory/kheap.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <drivers/com1.h>
#include <utility/align.h>
#include <utility/kstring.h>

#define ALIGNMENT_BYTES 16
#define MINIMUM_EXPAND 8
#define KERNEL_HEAP_PADDING 0x200000

#define INITIAL_SIZE (16 * 4096)
#define MIN_ALLOC    16

extern virt_addr_t* kernel_pml4;
extern virt_addr_t kernel_virt_base;
extern uint64_t kernel_size;

typedef struct heap_block {
    size_t size;
    bool is_free;
    struct heap_block* next;
    struct heap_block* prev;
} heap_block_t;

static virt_addr_t kheap_start;
static heap_block_t* heap_head = NULL;
static heap_block_t* heap_tail = NULL;
static virt_addr_t heap_current_max;

static size_t align_to_size(size_t size) {
    return (size + ALIGNMENT_BYTES - 1) & ~(ALIGNMENT_BYTES - 1);
}

static void kheap_expand(size_t size);

void kheap_init() {
    if (!kernel_virt_base || !kernel_size) {
        serial_printf("[KHEAP](kheap_init) Error while init. Kernel address and size not set. Make sure PMM is initialized.\n");
        while(1);
    }

    virt_addr_t kernel_end = kernel_virt_base + ALIGN_UP(kernel_size, PAGE_SIZE);
    kheap_start = ALIGN_UP(kernel_end + KERNEL_HEAP_PADDING, PAGE_SIZE);
    heap_current_max = kheap_start;

    kheap_expand(INITIAL_SIZE);
    serial_printf("[KHEAP](kheap_init) Initialized at 0x%x with size %d bytes\n", 
                  kheap_start, INITIAL_SIZE);
}

static void kheap_expand(size_t size) {
    size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    if (pages_needed < MINIMUM_EXPAND) pages_needed = MINIMUM_EXPAND;
    virt_addr_t region_base = heap_current_max;

    for (size_t i = 0; i < pages_needed; i++) {
        phys_addr_t phys = pmm_alloc(1);
        if (phys == 0) {
            serial_printf("[KHEAP](kheap_expand) Panic: OOM during expansion.\n");
            while(1);
        }

        vmm_map_page(
            (phys_addr_t*)kernel_pml4, 
            heap_current_max, phys, PT_FLAG_PRESENT | PT_FLAG_WRITE
        );

        memset((void*)heap_current_max, 0, PAGE_SIZE);
        heap_current_max += PAGE_SIZE;
    }

    heap_block_t* new_region = (heap_block_t*)region_base;

    new_region->size = (pages_needed * PAGE_SIZE) - sizeof(heap_block_t);
    new_region->is_free = true;
    new_region->prev = heap_tail;
    new_region->next = NULL;

    if (heap_tail) {
        heap_tail->next = new_region;
    } else {
        heap_head = new_region;
    }

    heap_tail = new_region;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    size_t aligned_size = align_to_size(size);
    heap_block_t* current = heap_head;

    while (current != NULL) {
        if (current->is_free && current->size >= aligned_size) {
            if (current->size > aligned_size + sizeof(heap_block_t) + MIN_ALLOC) {
                
                heap_block_t* new_block = (heap_block_t*)((uintptr_t)current + sizeof(heap_block_t) + aligned_size);
                
                new_block->size = current->size - aligned_size - sizeof(heap_block_t);
                new_block->is_free = true;
                new_block->next = current->next;
                new_block->prev = current;
                
                current->size = aligned_size;
                current->next = new_block;
                
                if (new_block->next) {
                    new_block->next->prev = new_block;
                }
                
                if (current == heap_tail) {
                    heap_tail = new_block;
                }
            }

            current->is_free = false;
            return (void*)((uintptr_t)current + sizeof(heap_block_t));
        }
        current = current->next;
    }

    serial_printf("[KHEAP] Heap full, expanding...\n");
    kheap_expand(aligned_size + sizeof(heap_block_t));
    return kmalloc(size); 
}

void kfree(void* ptr) {
    if (ptr == NULL) return;

    heap_block_t* block = (heap_block_t*)((uintptr_t)ptr - sizeof(heap_block_t));
    block->is_free = true;

    if (block->next && block->next->is_free) {
        heap_block_t* next_block = block->next;
        block->size += sizeof(heap_block_t) + next_block->size;
        block->next = next_block->next;
        
        if (block->next) {
            block->next->prev = block;
        } else {
            heap_tail = block;
        }
    }

    if (block->prev && block->prev->is_free) {
        heap_block_t* prev_block = block->prev;
        prev_block->size += sizeof(heap_block_t) + block->size;
        prev_block->next = block->next;
        
        if (block->next) {
            block->next->prev = prev_block;
        } else {
            heap_tail = prev_block;
        }
    }
}