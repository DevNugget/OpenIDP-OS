#include <drivers/acpi.h>
#include <drivers/com1.h>
#include <drivers/apic.h>
#include <utility/hhdm.h>
#include <memory/vmm.h>
#include <limine.h>

#include <stdbool.h>
#include <stddef.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

extern virt_addr_t* kernel_pml4;
static volatile madt_io_apic_t* io_apic;

bool sdt_cmp_signature(acpi_sdt_header_t* header, char* signature) {
    return (
        header->Signature[0] == signature[0] &&
        header->Signature[1] == signature[1] &&
        header->Signature[2] == signature[2] &&
        header->Signature[3] == signature[3]
    );
}

void* sdt_find(rsdp_descriptor_t* rsdp, char* signature) {
    if (rsdp->Revision >= 2) { // xsdt
        rsdp2_descriptor_t* rsdp2 = (rsdp2_descriptor_t*)rsdp;
        xsdt_t* xsdt = (xsdt_t*)phys_to_virt(rsdp2->XSDTAddress);

        size_t entry_count = (xsdt->sdtHeader.Length - sizeof(acpi_sdt_header_t)) / 8;
        for (size_t i = 0; i < entry_count; i++) {
            acpi_sdt_header_t* header = 
            (acpi_sdt_header_t*)phys_to_virt(xsdt->sdtAddresses[i]);

            if (sdt_cmp_signature(header, signature)) {
                return (void*)header;
            }
        }
    } else { // sdt
        rsdt_t* rsdt = (rsdt_t*)phys_to_virt(rsdp->RsdtAddress);

        size_t entry_count = (rsdt->sdtHeader.Length - sizeof(acpi_sdt_header_t)) / 4;
        for (size_t i = 0; i < entry_count; i++) {
            acpi_sdt_header_t* header = 
            (acpi_sdt_header_t*)phys_to_virt(rsdt->sdtAddresses[i]);

            if (sdt_cmp_signature(header, signature)) {
                return (void*)header;
            }
        }
    }

    return NULL;
}

void acpi_parse_madt(rsdp_descriptor_t* rsdp) {
    madt_t* madt = (madt_t*)sdt_find(rsdp, "APIC");
    if (!madt) {
        serial_printf("[ACPI](acpi_parse_madt) MADT not found!\n");
        return;
    }
    serial_printf("[ACPI](acpi_parse_madt) MADT found!\n");

    uint8_t* start = (uint8_t*)madt->entries;
    uint8_t* end = (uint8_t*)madt + madt->header.Length;

    while (start < end) {
        madt_header_t* entry = (madt_header_t*)start;

        if (entry->type == MADT_TYPE_IO_ACPI) {
            io_apic = (madt_io_apic_t*)entry;
            serial_printf("[ACPI](acpi_parse_madt) I/O APIC found @(PHYS)0x%x GSI Base: %d\n", 
                io_apic->io_apic_addr, io_apic->gsi_base);
        } else if (entry->type == MADT_TYPE_ISO) {
            madt_iso_t* iso = (madt_iso_t*)entry;
            serial_printf("[ACPI] ISO: IRQ %d -> GSI %d\n", iso->irq_src, iso->gsi);
        }

        start += entry->length;
    }
}

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

    acpi_parse_madt(rsdp);
    io_apic_init(io_apic->io_apic_addr);
}