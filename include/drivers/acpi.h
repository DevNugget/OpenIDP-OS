/* date = February 12th 2026 10:49 pm */

#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>
#include <stdbool.h>

/* RSDP - Root System Description Pointer */
typedef struct rsdp_descriptor {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
} __attribute__ ((packed)) rsdp_descriptor_t;

typedef struct rsdp2_descriptor {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
    
    uint32_t Length;
    uint64_t XSDTAddress;
    uint8_t ExtendedChecksum;
    uint8_t Reserved[3];
} __attribute__ ((packed)) rsdp2_descriptor_t;

typedef struct acpi_sdt_header {
    char Signature[4];
    uint32_t Length; // NOTE: size of table w/ header
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__ ((packed)) acpi_sdt_header_t;

typedef struct rsdt {
    acpi_sdt_header_t sdtHeader; //signature "RSDT"
    uint32_t sdtAddresses[];
} __attribute__ ((packed)) rsdt_t;

typedef struct xsdt {
    acpi_sdt_header_t sdtHeader; //signature "XSDT"
    uint64_t sdtAddresses[];
} __attribute__ ((packed)) xsdt_t;

#define MADT_TYPE_IO_ACPI 1
#define MADT_TYPE_ISO 2

typedef struct madt {
    acpi_sdt_header_t header;
    uint32_t local_apic_addr;
    uint32_t flags;
    uint8_t entries[];
} __attribute__ ((packed)) madt_t;

typedef struct madt_header {
    uint8_t type;
    uint8_t length;
} __attribute__ ((packed)) madt_header_t;

typedef struct madt_io_apic {
    madt_header_t header;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t io_apic_addr;
    uint32_t gsi_base;
} __attribute__ ((packed)) madt_io_apic_t;

typedef struct madt_iso {
    madt_header_t h;
    uint8_t bus_src;
    uint8_t irq_src;
    uint32_t gsi;
    uint16_t flags;
} __attribute__ ((packed)) madt_iso_t;

void acpi_init();

#endif //ACPI_H
