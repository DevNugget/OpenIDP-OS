#include <drivers/acpi.h>
#include <limine.h>

#include <stdbool.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

bool validate_RSDP(char *byte_array, size_t size) {
    uint32_t sum = 0;
    for(int i = 0; i < size; i++) {
        sum += byte_array[i];
    }
    return (sum & 0xFF) == 0;
}