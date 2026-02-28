/* date = February 27th 2026 5:08 pm */
#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t header_type;
    uint8_t revision_id;
} pci_device_info_t;

void pci_init(void);

uint32_t pci_config_read_u32(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_offset);
uint16_t pci_config_read_u16(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_offset);
uint8_t pci_config_read_u8(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_offset);
void pci_config_write_u32(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_offset, uint32_t value);

bool pci_find_first_by_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if, pci_device_info_t* out_device);
uint64_t pci_read_bar(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar_index, bool* is_memory_space, bool* is_64bit);

#endif //PCI_H