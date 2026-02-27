#include <drivers/pci.h>
#include <drivers/com1.h>
#include <utility/port.h>
#include <stdbool.h>

#define PCI_CONFIG_ADDR_PORT 0xCF8
#define PCI_CONFIG_DATA_PORT 0xCFC

#define PCI_INVALID_VENDOR 0xFFFF

#define PCI_HEADER_TYPE_MULTIFUNCTION 0x80
#define PCI_HEADER_TYPE_MASK 0x7F
#define PCI_HEADER_TYPE_PCI_TO_PCI_BRIDGE 0x01

static uint32_t pci_config_read_u32(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_offset) {
    uint32_t address =
        (1U << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)function << 8) |
        (reg_offset & 0xFC);

    outportl(PCI_CONFIG_ADDR_PORT, address);
    return inportl(PCI_CONFIG_DATA_PORT);
}

static uint16_t pci_config_read_u16(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_offset) {
    uint32_t value = pci_config_read_u32(bus, device, function, reg_offset);
    return (value >> ((reg_offset & 2) * 8)) & 0xFFFF;
}

static uint8_t pci_config_read_u8(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_offset) {
    uint32_t value = pci_config_read_u32(bus, device, function, reg_offset);
    return (value >> ((reg_offset & 3) * 8)) & 0xFF;
}

static void pci_scan_bus(uint8_t bus);

static void pci_scan_function(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor_id = pci_config_read_u16(bus, device, function, 0x00);
    if (vendor_id == PCI_INVALID_VENDOR) {
        return;
    }

    uint16_t device_id = pci_config_read_u16(bus, device, function, 0x02);
    uint8_t class_code = pci_config_read_u8(bus, device, function, 0x0B);
    uint8_t subclass = pci_config_read_u8(bus, device, function, 0x0A);
    uint8_t prog_if = pci_config_read_u8(bus, device, function, 0x09);
    uint8_t header_type = pci_config_read_u8(bus, device, function, 0x0E) & PCI_HEADER_TYPE_MASK;

    serial_printf(
        "[PCI] %x:%x.%x vendor=%x device=%x class=%x subclass=%x prog_if=%x\n",
        bus,
        device,
        function,
        vendor_id,
        device_id,
        class_code,
        subclass,
        prog_if
    );

    if (header_type == PCI_HEADER_TYPE_PCI_TO_PCI_BRIDGE) {
        uint8_t secondary_bus = pci_config_read_u8(bus, device, function, 0x19);
        if (secondary_bus != 0) {
            serial_printf("[PCI] Bridge %x:%x.%x -> secondary bus %x\n", bus, device, function, secondary_bus);
            pci_scan_bus(secondary_bus);
        }
    }
}

static void pci_scan_device(uint8_t bus, uint8_t device) {
    uint16_t vendor_id = pci_config_read_u16(bus, device, 0, 0x00);
    if (vendor_id == PCI_INVALID_VENDOR) {
        return;
    }

    uint8_t header_type = pci_config_read_u8(bus, device, 0, 0x0E);
    uint8_t function_count = (header_type & PCI_HEADER_TYPE_MULTIFUNCTION) ? 8 : 1;

    for (uint8_t function = 0; function < function_count; function++) {
        pci_scan_function(bus, device, function);
    }
}

static void pci_scan_bus(uint8_t bus) {
    for (uint8_t device = 0; device < 32; device++) {
        pci_scan_device(bus, device);
    }
}

void pci_init(void) {
    serial_write_str("[PCI] Enumerating PCI buses\n");

    uint8_t header_type = pci_config_read_u8(0, 0, 0, 0x0E);
    if ((header_type & PCI_HEADER_TYPE_MULTIFUNCTION) == 0) {
        pci_scan_bus(0);
        return;
    }

    for (uint8_t function = 0; function < 8; function++) {
        uint16_t vendor_id = pci_config_read_u16(0, 0, function, 0x00);
        if (vendor_id == PCI_INVALID_VENDOR) {
            continue;
        }

        pci_scan_bus(function);
    }
}
