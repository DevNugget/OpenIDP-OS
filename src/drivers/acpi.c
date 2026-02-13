#include <drivers/acpi.h>
#include <drivers/com1.h>
#include <utility/hhdm.h>
#include <limine.h>

#include <stdbool.h>
#include <stddef.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

bool validate_rsdp(uint8_t *byte_array, size_t size) {
    uint32_t sum = 0;
    for(size_t i = 0; i < size; i++) {
        sum += byte_array[i];
    }
    return (sum & 0xFF) == 0;
}

void acpi_init() {
    if (rsdp_request.response == NULL ||
        rsdp_request.response->address == 0x0) {
        serial_write_str("[RSDP](acpi_init) Pointer not found");
        return;
    }
    
    rsdp_descriptor_t* rsdp = (rsdp_descriptor_t*)rsdp_request.response->address;
    
    bool rsdp_valid = false;
    if (rsdp->Revision == 0) {
        rsdp_valid = validate_rsdp((uint8_t*)rsdp, sizeof(rsdp_descriptor_t));
        serial_write_str("[ACPI](acpi_init) Detected ACPI 1.0\n");
    } else {
        rsdp2_descriptor_t* rsdp2 = (rsdp2_descriptor_t*)rsdp;
        rsdp_valid = validate_rsdp((uint8_t*)rsdp2, sizeof(rsdp2_descriptor_t));
        serial_write_str("[ACPI](acpi_init) Detected ACPI 2.0\n");
    }
    
    if (!rsdp_valid) {
        serial_write_str("[RSDP](acpi_init) Pointer validation failed\n");
        return;
    }
    serial_write_str("[RSDP](acpi_init) Pointer validated\n");
}