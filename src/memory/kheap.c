#include <memory/kheap.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <drivers/com1.h>
#include <utility/align.h>
#include <utility/kstring.h>

#define KERNEL_HEAP_PADDING 0x200000
#define MINIMUM_EXPAND_PAGES 8
#define SLAB_MAGIC 0x534C4142u
#define LARGE_MAGIC 0x4C415247u
#define MAX_SLAB_CLASSES 8

typedef struct slab_page {
    uint32_t magic;
    uint16_t class_idx;
    uint16_t capacity;
    uint16_t used;
    uint16_t bitmap_words;
    struct slab_page* next;
} slab_page_t;

typedef struct {
    size_t object_size;
    slab_page_t* pages;
} slab_class_t;

typedef struct large_alloc_header {
    uint32_t magic;
    uint32_t page_count;
    size_t requested_size;
} large_alloc_header_t;

static const size_t slab_sizes[MAX_SLAB_CLASSES] = {
    16, 32, 64, 128, 256, 512, 1024, 2048
};

static slab_class_t slab_classes[MAX_SLAB_CLASSES];
static virt_addr_t kheap_start;
static virt_addr_t heap_current_max;

extern virt_addr_t* kernel_pml4;
extern virt_addr_t kernel_virt_base;
extern uint64_t kernel_size;

static inline virt_addr_t page_align_down(virt_addr_t addr) {
    return addr & ~(PAGE_SIZE - 1);
}

static inline uint64_t* slab_bitmap(slab_page_t* page) {
    return (uint64_t*)((uintptr_t)page + sizeof(slab_page_t));
}

static void panic_oom(const char* source) {
    serial_printf("[KHEAP](%s) Panic: OOM.\n", source);
    while (1);
}

static virt_addr_t map_heap_pages(size_t page_count) {
    if (page_count < MINIMUM_EXPAND_PAGES) {
        page_count = MINIMUM_EXPAND_PAGES;
    }

    virt_addr_t region_base = heap_current_max;

    for (size_t i = 0; i < page_count; i++) {
        phys_addr_t phys = pmm_alloc(1);
        if (!phys) {
            panic_oom("map_heap_pages");
        }

        vmm_map_page((phys_addr_t*)kernel_pml4,
                     heap_current_max,
                     phys,
                     PT_FLAG_PRESENT | PT_FLAG_WRITE);

        memset((void*)heap_current_max, 0, PAGE_SIZE);
        heap_current_max += PAGE_SIZE;
    }

    return region_base;
}

static uint16_t slab_capacity_for_class(size_t object_size, uint16_t* out_words) {
    size_t header_size = sizeof(slab_page_t);
    uint16_t max_capacity = (PAGE_SIZE - header_size) / object_size;

    for (uint16_t cap = max_capacity; cap > 0; cap--) {
        uint16_t words = (cap + 63) / 64;
        size_t bitmap_size = (size_t)words * sizeof(uint64_t);
        size_t required = header_size + bitmap_size + (size_t)cap * object_size;

        if (required <= PAGE_SIZE) {
            *out_words = words;
            return cap;
        }
    }

    *out_words = 0;
    return 0;
}

static slab_page_t* slab_new_page(uint16_t class_idx) {
    virt_addr_t base = map_heap_pages(1);
    slab_page_t* page = (slab_page_t*)base;
    uint16_t words = 0;
    uint16_t capacity = slab_capacity_for_class(slab_classes[class_idx].object_size, &words);

    if (!capacity) {
        serial_printf("[KHEAP](slab_new_page) Invalid capacity for class %u\n", class_idx);
        while (1);
    }

    page->magic = SLAB_MAGIC;
    page->class_idx = class_idx;
    page->capacity = capacity;
    page->used = 0;
    page->bitmap_words = words;
    page->next = slab_classes[class_idx].pages;

    slab_classes[class_idx].pages = page;
    return page;
}

static int slab_first_free_idx(slab_page_t* page) {
    uint64_t* bitmap = slab_bitmap(page);

    for (uint16_t word_idx = 0; word_idx < page->bitmap_words; word_idx++) {
        if (bitmap[word_idx] == UINT64_MAX) {
            continue;
        }

        for (uint16_t bit = 0; bit < 64; bit++) {
            uint16_t idx = (word_idx * 64) + bit;
            if (idx >= page->capacity) {
                return -1;
            }

            if (!(bitmap[word_idx] & (1ULL << bit))) {
                return idx;
            }
        }
    }

    return -1;
}

static void* slab_alloc(uint16_t class_idx) {
    slab_page_t* page = slab_classes[class_idx].pages;

    while (page && page->used == page->capacity) {
        page = page->next;
    }

    if (!page) {
        page = slab_new_page(class_idx);
    }

    int idx = slab_first_free_idx(page);
    if (idx < 0) {
        serial_printf("[KHEAP](slab_alloc) Corrupt slab bitmap for class %u\n", class_idx);
        while (1);
    }

    uint64_t* bitmap = slab_bitmap(page);
    uint16_t word_idx = (uint16_t)idx / 64;
    uint16_t bit_idx = (uint16_t)idx % 64;
    bitmap[word_idx] |= (1ULL << bit_idx);
    page->used++;

    uintptr_t objects_start = (uintptr_t)page + sizeof(slab_page_t) + ((size_t)page->bitmap_words * sizeof(uint64_t));
    return (void*)(objects_start + ((size_t)idx * slab_classes[class_idx].object_size));
}

static void slab_free(void* ptr) {
    slab_page_t* page = (slab_page_t*)page_align_down((virt_addr_t)ptr);

    if (page->magic != SLAB_MAGIC || page->class_idx >= MAX_SLAB_CLASSES) {
        serial_printf("[KHEAP](kfree) Invalid slab pointer 0x%x\n", (uint64_t)ptr);
        return;
    }

    uintptr_t objects_start = (uintptr_t)page + sizeof(slab_page_t) + ((size_t)page->bitmap_words * sizeof(uint64_t));
    size_t object_size = slab_classes[page->class_idx].object_size;
    uintptr_t ptr_addr = (uintptr_t)ptr;

    if (ptr_addr < objects_start) {
        serial_printf("[KHEAP](kfree) Slab pointer underflow 0x%x\n", (uint64_t)ptr);
        return;
    }

    size_t offset = ptr_addr - objects_start;
    if (offset % object_size != 0) {
        serial_printf("[KHEAP](kfree) Slab pointer misaligned 0x%x\n", (uint64_t)ptr);
        return;
    }

    uint16_t idx = offset / object_size;
    if (idx >= page->capacity) {
        serial_printf("[KHEAP](kfree) Slab pointer out of range 0x%x\n", (uint64_t)ptr);
        return;
    }

    uint64_t* bitmap = slab_bitmap(page);
    uint16_t word_idx = idx / 64;
    uint16_t bit_idx = idx % 64;

    if (!(bitmap[word_idx] & (1ULL << bit_idx))) {
        serial_printf("[KHEAP](kfree) Double free detected for 0x%x\n", (uint64_t)ptr);
        return;
    }

    bitmap[word_idx] &= ~(1ULL << bit_idx);
    if (page->used) {
        page->used--;
    }
}

void kheap_init() {
    if (!kernel_virt_base || !kernel_size) {
        serial_printf("[KHEAP](kheap_init) Error while init. Kernel address and size not set. Make sure PMM is initialized.\n");
        while (1);
    }

    virt_addr_t kernel_end = kernel_virt_base + ALIGN_UP(kernel_size, PAGE_SIZE);
    kheap_start = ALIGN_UP(kernel_end + KERNEL_HEAP_PADDING, PAGE_SIZE);
    heap_current_max = kheap_start;

    for (uint16_t i = 0; i < MAX_SLAB_CLASSES; i++) {
        slab_classes[i].object_size = slab_sizes[i];
        slab_classes[i].pages = NULL;
    }

    map_heap_pages(MINIMUM_EXPAND_PAGES);

    serial_printf("[KHEAP](kheap_init) Slab allocator initialized at 0x%x\n", kheap_start);
}

void* kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    for (uint16_t i = 0; i < MAX_SLAB_CLASSES; i++) {
        if (size <= slab_classes[i].object_size) {
            return slab_alloc(i);
        }
    }

    size_t total = size + sizeof(large_alloc_header_t);
    size_t pages = ALIGN_UP(total, PAGE_SIZE) / PAGE_SIZE;
    virt_addr_t base = map_heap_pages(pages);

    large_alloc_header_t* header = (large_alloc_header_t*)base;
    header->magic = LARGE_MAGIC;
    header->page_count = pages;
    header->requested_size = size;

    return (void*)((uintptr_t)header + sizeof(large_alloc_header_t));
}

void kfree(void* ptr) {
    if (!ptr) {
        return;
    }

    virt_addr_t page_base = page_align_down((virt_addr_t)ptr);
    uint32_t magic = *(uint32_t*)page_base;

    if (magic == SLAB_MAGIC) {
        slab_free(ptr);
        return;
    }

    if (magic == LARGE_MAGIC) {
        large_alloc_header_t* header = (large_alloc_header_t*)page_base;
        header->magic = 0;
        return;
    }

    serial_printf("[KHEAP](kfree) Unknown allocation type for pointer 0x%x\n", (uint64_t)ptr);
}