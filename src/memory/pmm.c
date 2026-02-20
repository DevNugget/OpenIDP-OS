#include <memory/pmm.h>
#include <drivers/com1.h>
#include <utility/kstring.h>
#include <limine.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request executable_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_file_request executable_file_request = {
    .id = LIMINE_EXECUTABLE_FILE_REQUEST_ID,
    .revision = 0
};

static inline uint64_t bitmap_idx(uint64_t x, uint64_t y) {
    return ((x * BITS_PER_ROW) + y);
}

static inline uint64_t bitmap_addr(uint64_t x, uint64_t y) {
    return (bitmap_idx(x, y) * PAGE_SIZE);
}

static inline uint64_t bitmap_location(phys_addr_t addr) {
    return (addr / PAGE_SIZE);
}

static inline uint64_t bitmap_addr_row(phys_addr_t addr) {
    return (bitmap_location(addr) / BITS_PER_ROW);
}

static inline uint64_t bitmap_addr_col(phys_addr_t addr) {
    return (bitmap_location(addr) % BITS_PER_ROW);
}

static inline uint64_t pages_to_mib(uint64_t pages) {
    return pages >> 8;
}

uint64_t kernel_size;
virt_addr_t kernel_virt_base;

/* PMM */
static virt_addr_t* bitmap;
static uint64_t total_pages;
static uint64_t used_pages;

void pmm_free_page(uint64_t addr) {
    uint64_t row = bitmap_addr_row(addr);
    uint64_t col = bitmap_addr_col(addr);
    bitmap[row] &= ~(1ULL << col);
    used_pages--;
}

void pmm_set_page(uint64_t addr) {
    uint64_t row = bitmap_addr_row(addr);
    uint64_t col = bitmap_addr_col(addr);
    bitmap[row] |= (1ULL << col);
    used_pages++;
}

bool pmm_page_taken(uint64_t addr) {
    uint64_t row = bitmap_addr_row(addr);
    uint64_t col = bitmap_addr_col(addr);
    return (bitmap[row] & (1ULL << col)) != 0;
}

void bitmap_unset(uint64_t idx) {
    uint64_t row = idx / BITS_PER_ROW;
    uint64_t col = idx % BITS_PER_ROW;
    bitmap[row] &= ~(1ULL << col);
    used_pages--;
}

void bitmap_set(uint64_t idx) {
    uint64_t row = idx / BITS_PER_ROW;
    uint64_t col = idx % BITS_PER_ROW;
    bitmap[row] |= (1ULL << col);
    used_pages++;
}

bool bitmap_test(uint64_t idx) {
    uint64_t row = idx / BITS_PER_ROW;
    uint64_t col = idx % BITS_PER_ROW;
    return (bitmap[row] & (1ULL << col)) != 0;
}

void pmm_init() {
    struct limine_memmap_response* memmap = memmap_request.response;
    
    if (memmap == NULL || memmap->entry_count < 1) {
        serial_write_str("[PMM](pmm_init) Error getting memmap from Limine.\n");
        serial_write_str("[PMM](pmm_init) Hanging\n");
        while(1);
    }
    
    phys_addr_t max_addr = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        
        if (entry->type == LIMINE_MEMMAP_USABLE
            /* || entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE*/) {
            phys_addr_t top = entry->base + entry->length;
            if (top > max_addr) {
                max_addr = top;
            }
        }
    }

    total_pages = max_addr / PAGE_SIZE;
    serial_printf(
        "[PMM](pmm_init) Total pages detected: %u (%u.%u MiB)\n", 
        total_pages, pages_to_mib(total_pages), ((total_pages % 256) * 4)
    );
    
    uint64_t bytes_needed = (total_pages + 7) / 8;
    serial_printf("[PMM](pmm_init) Bytes needed for bitmap: %u\n", bytes_needed);

    phys_addr_t bitmap_phys = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            if (entry->length >= bytes_needed) {
                bitmap_phys = entry->base;
                bitmap = (virt_addr_t*)phys_to_virt(entry->base);
                serial_printf("[PMM](pmm_init) Bitmap placed at (PHYS)0x%x @ (VIRT)0x%x\n", 
                    bitmap_phys, (uint64_t)bitmap
                );

                memset(bitmap, 0xFF, bytes_needed);
                break;
            }
        }
    }

    used_pages = total_pages;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];

        uint64_t entry_size = entry->base + entry->length;
        for (uint64_t j = entry->base; j < entry_size; j += PAGE_SIZE) {
            if (entry->type == LIMINE_MEMMAP_USABLE) {
                pmm_free_page(j);
            }
        }
    }

    uint64_t free_pages = total_pages - used_pages;
    uint64_t mib_free = pages_to_mib(free_pages);
    serial_printf(
        "[PMM](pmm_init) Usable pages marked free: %u (%u.%u MiB)\n",
        free_pages, mib_free, ((used_pages % 256) * 4)
    );

    for (uint64_t i = 0; i < bytes_needed; i += PAGE_SIZE) {
        pmm_set_page(bitmap_phys + i);
    }
    serial_printf("[PMM](pmm_init) Locked bitmap pages.\n");

    kernel_size = executable_file_request.response->executable_file->size;
    kernel_virt_base = executable_address_request.response->virtual_base;
    uint64_t kernel_base = executable_address_request.response->physical_base;
    serial_printf("[PMM](pmm_init) Kernel base found at (PHYS)0x%x of (SIZE)%d bytes\n", kernel_base, kernel_size);

    for (uint64_t i = 0; i < kernel_size; i+= PAGE_SIZE) {
        pmm_set_page(kernel_base + i);
    }
    serial_printf("[PMM](pmm_init) Locked kernel pages.\n");

    serial_printf("[PMM](pmm_init) [TODO]: Lock other important memory.\n");

    serial_printf(
        "[PMM](pmm_init) Usage after initialization: (%u pages/%u pages) (%u.%u MiB/%u.%u MiB)\n",
        used_pages, total_pages, pages_to_mib(used_pages), 
        ((used_pages % 256) * 4), pages_to_mib(total_pages), ((total_pages % 256) * 4)
    );
}

phys_addr_t pmm_alloc(size_t frame_count) {
    uint64_t start_bit = 0;
    uint64_t free_count = 0;

    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (free_count == 0) {
                start_bit = i;
            }
            free_count++;

            if (free_count == frame_count) {
                for (uint64_t j = 0; j < frame_count; j++) {
                    bitmap_set(start_bit + j);
                }

                phys_addr_t address = start_bit * PAGE_SIZE;
                serial_printf(
                    "[PMM](pmm_alloc) Usage after alloc @(PHYS)0x%x + %u: (%u pages/%u pages) (%u.%u MiB/%u.%u MiB)\n",
                    address, frame_count, used_pages, total_pages, pages_to_mib(used_pages), 
                    ((used_pages % 256) * 4), pages_to_mib(total_pages), ((total_pages % 256) * 4)
                );
                return address;
            }
        } else {
            free_count = 0;
        }
    }

    return 0x0;
}

void pmm_free(phys_addr_t addr, size_t frame_count) {
    uint64_t start_idx = bitmap_location(addr);

    for (uint64_t i = 0; i < frame_count; i++) {
        bitmap_unset(start_idx + i);
    }
    serial_printf(
        "[PMM](pmm_free) Usage after %u page free: (%u pages/%u pages) (%u.%u MiB/%u.%u MiB)\n",
        frame_count, used_pages, total_pages, pages_to_mib(used_pages), 
        ((used_pages % 256) * 4), pages_to_mib(total_pages), ((total_pages % 256) * 4)
    );
}