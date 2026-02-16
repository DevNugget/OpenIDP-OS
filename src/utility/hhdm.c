#include <utility/hhdm.h>
#include <drivers/com1.h>
#include <limine.h>
#include <stddef.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

static uint64_t hhdm_offset = 0x0;

uint64_t get_hhdm() {
    return hhdm_offset;
}

void hhdm_request_offset() {
    if (hhdm_request.response == 0x0) {
        serial_write_str("[HHDM](request_offset) No offset response from Limine\n");
    }
    hhdm_offset = hhdm_request.response->offset;
    serial_printf("[HHDM](request_offset=0x%x) Offset response from Limine OK\n", hhdm_offset);
}

void* phys_to_virt(uint64_t phys) {
    if (hhdm_offset == 0x0) {
        serial_write_str("[HHDM](phys_to_virt) No HHDM offset. Did you call hhdm_request_offset()?\n");
    }
    
    return (void*)(phys+hhdm_offset);
}