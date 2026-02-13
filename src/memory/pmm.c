#include <memory/pmm.h>
#include <drivers/com1.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

static inline uint64_t bitmap_idx(uint64_t x, uint64_t y) {
    return ((x * BITS_PER_ROW) + y);
}

static inline uint64_t bitmap_addr(uint64_t x, uint64_t y) {
    return (bitmap_idx(x, y) * PAGE_SIZE);
}

static inline uint64_t bitmap_location(uint64_t addr) {
    return (addr / PAGE_SIZE);
}

static inline uint64_t bitmap_addr_row(uint64_t addr) {
    return (bitmap_location(addr) / BITS_PER_ROW);
}

static inline uint64_t bitmap_addr_col(uint64_t addr) {
    return (bitmap_location(addr) % BITS_PER_ROW);
}

/* PMM */
static uint64_t* bitmap;

void pmm_init() {
    struct limine_memmap_response* memmap = memmap_request.response;
    
    if (memmap == NULL || memmap->entry_count < 1) {
        serial_write_str("[PMM](pmm_init) Error getting memmap from Limine.\n");
        serial_write_str("[PMM](pmm_init) Hanging\n");
        while(1);
    }
    
    uint64_t max_addr = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        
        if (entry->type == LIMINE_MEMMAP_USABLE ||
            entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            uint64_t top = entry->base + entry->length;
            if (top > max_addr) {
                max_addr = top;
            }
        }
    }
}