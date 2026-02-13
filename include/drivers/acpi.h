/* date = February 12th 2026 10:49 pm */

#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

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

void acpi_init();

#endif //ACPI_H
