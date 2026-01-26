#include <pmm.h>

#define PAGE_SIZE 4096
#define BITS_PER_BYTE 8

static uint8_t*  pmm_bitmap;
static uint64_t  pmm_bitmap_phys;

static uint64_t  total_pages;
static uint64_t  used_pages;

static uint64_t  bitmap_size_bytes;
static uint64_t  bitmap_size_pages;

uint64_t limine_hhdm;

static inline void bitmap_set(uint64_t bit) {
    pmm_bitmap[bit >> 3] |= (1u << (bit & 7u));
}

static inline void bitmap_clear(uint64_t bit) {
    pmm_bitmap[bit >> 3] &= ~(1u << (bit & 7u));
}

static inline int bitmap_test(uint64_t bit) {
    return pmm_bitmap[bit >> 3] & (1u << (bit & 7u));
}

static inline uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static inline uint64_t align_down(uint64_t value, uint64_t alignment) {
    return value & ~(alignment - 1);
}

static void count_total_pages(struct limine_memmap_response* memmap) {
    total_pages = 0;
    
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* e = memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE) {
            total_pages += e->length / PAGE_SIZE;
        }
    }
    
    bitmap_size_bytes = (total_pages + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
    bitmap_size_pages = align_up(bitmap_size_bytes, PAGE_SIZE) / PAGE_SIZE;
}

static void place_bitmap(struct limine_memmap_response* memmap,
                         struct limine_hhdm_response* hhdm) {
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* e = memmap->entries[i];

        if (e->type == LIMINE_MEMMAP_USABLE &&
            e->length >= bitmap_size_pages * PAGE_SIZE) {
            
            pmm_bitmap_phys = align_up(e->base, PAGE_SIZE);
            pmm_bitmap = (uint8_t*)(pmm_bitmap_phys + hhdm->offset);
            return;
        }
    }

    serial_printf("PMM PANIC: Could not place bitmap\n");
    while (1);
}

static void free_usable_pages(struct limine_memmap_response* memmap) {
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* e = memmap->entries[i];
        
        if (e->type != LIMINE_MEMMAP_USABLE) continue;
        
        uint64_t start_page = align_up(e->base, PAGE_SIZE) / PAGE_SIZE;
        uint64_t end_page = align_down(e->base + e->length, PAGE_SIZE) / PAGE_SIZE;
        
        if (start_page >= end_page) continue;
        
        for (uint64_t page = start_page; page < end_page; page++) {
            if (bitmap_test(page)) {
                bitmap_clear(page);
                used_pages--;
            }
        }
    }
}

static void lock_pages_range(uint64_t start_phys, uint64_t end_phys) {
    uint64_t start_page = align_up(start_phys, PAGE_SIZE) / PAGE_SIZE;
    uint64_t end_page = align_down(end_phys, PAGE_SIZE) / PAGE_SIZE;
    
    if (start_page >= end_page) return;
    
    for (uint64_t page = start_page; page < end_page; page++) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            used_pages++;
        }
    }
}

static void lock_kernel_pages(struct limine_executable_address_response* kaddr) {
    extern uint8_t _kernel_physical_end[];
    extern uint8_t _kernel_physical_start[];
    
    uint64_t kernel_size = (uint64_t)_kernel_physical_end - (uint64_t)_kernel_physical_start;

    uint64_t start = kaddr->physical_base;
    uint64_t end = start + kernel_size;
    
    lock_pages_range(start, end);
    serial_printf("PMM: Locked kernel pages [%x - %x]\n", start, end);
}

static void lock_bitmap_pages(void) {
    uint64_t start = pmm_bitmap_phys;
    uint64_t end = start + bitmap_size_pages * PAGE_SIZE;
    
    lock_pages_range(start, end);
    serial_printf("PMM: Locked %u bitmap pages at %x\n", 
                  bitmap_size_pages, pmm_bitmap_phys);
}

void pmm_init(struct limine_memmap_response* memmap,
              struct limine_executable_address_response* kernel,
              struct limine_hhdm_response* hhdm) {

    limine_hhdm = hhdm->offset;

    count_total_pages(memmap);
    place_bitmap(memmap, hhdm);

    memset(pmm_bitmap, 0xFF, bitmap_size_bytes);
    used_pages = total_pages;

    free_usable_pages(memmap);
    lock_kernel_pages(kernel);
    lock_bitmap_pages();

    serial_printf("PMM initialized: %u total, %u used, %u free\n",
                  total_pages, used_pages, total_pages - used_pages);
}

void* pmm_alloc_page() {
    // Quick check for availability
    if (used_pages >= total_pages) return NULL;
    
    // Search from last found position to avoid always starting from 0
    static uint64_t last_search_pos = 0;
    uint64_t start_pos = last_search_pos;
    
    do {
        if (!bitmap_test(last_search_pos)) {
            bitmap_set(last_search_pos);
            used_pages++;
            serial_printf("PMM: Allocated page %u (used: %u/%u)\n",
                          last_search_pos, used_pages, total_pages);
            return (void*)(last_search_pos * PAGE_SIZE + limine_hhdm);
        }
        
        last_search_pos = (last_search_pos + 1) % total_pages;
    } while (last_search_pos != start_pos);
    
    serial_printf("PMM WARNING: No free pages found despite availability check\n");
    return NULL;
}

void pmm_free_page(void* addr) {
    uint64_t phys = (uint64_t)addr - limine_hhdm;
    uint64_t page = phys / PAGE_SIZE;
    
    if (!bitmap_test(page)) {
        serial_printf("PMM WARNING: Double free detected at page %u\n", page);
        return;
    }
    
    bitmap_clear(page);
    used_pages--;
}

void* pmm_alloc_pages(size_t count) {
    if (count == 0 || used_pages + count > total_pages) return NULL;
    
    uint64_t consecutive = 0;
    uint64_t start_page = 0;
    
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (consecutive == 0) start_page = i;
            consecutive++;
            
            if (consecutive == count) {
                for (uint64_t j = 0; j < count; j++) {
                    bitmap_set(start_page + j);
                }
                used_pages += count;
                return (void*)(start_page * PAGE_SIZE + limine_hhdm);
            }
        } else {
            consecutive = 0;
        }
    }
    
    return NULL;
}

void pmm_free_pages(void* addr, size_t count) {
    if (count == 0) return;
    
    uint64_t phys = (uint64_t)addr - limine_hhdm;
    uint64_t start_page = phys / PAGE_SIZE;
    
    for (size_t i = 0; i < count; i++) {
        uint64_t page = start_page + i;
        if (bitmap_test(page)) {
            bitmap_clear(page);
            used_pages--;
        } else {
            serial_printf("PMM WARNING: Double free detected in batch at page %u\n", page);
        }
    }
}