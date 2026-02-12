/* date = February 12th 2026 10:49 pm */

#ifndef ACPI_H
#define ACPI_H

/* RSDP - Root System Description Pointer */
struct rsdp_descriptor {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
} __attribute__ ((packed)) rsdp_descriptor_t;

struct rsdp2_descriptor {
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

#endif //ACPI_H
