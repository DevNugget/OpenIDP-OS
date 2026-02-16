#include <drivers/apic.h>
#include <drivers/com1.h>
#include <utility/port.h>
#include <utility/hhdm.h>
#include <memory/vmm.h>

extern virt_addr_t* kernel_pml4;
volatile uint32_t* io_apic_base;

static inline uint64_t read_msr(uint32_t msr) {
    uint32_t lower;
    uint32_t upper;
    __asm__ volatile (
                      "rdmsr"
                      : "=a"(lower), "=d"(upper)
                      : "c"(msr)
                      );
    return ((uint64_t)upper << 32) | lower;
}

void disable_pic() {
    outportb(PIC_COMMAND_MASTER, ICW_1);
    outportb(PIC_COMMAND_SLAVE, ICW_1);
    outportb(PIC_DATA_MASTER, ICW_2_M);
    outportb(PIC_DATA_SLAVE, ICW_2_S);
    outportb(PIC_DATA_MASTER, ICW_3_M);
    outportb(PIC_DATA_SLAVE, ICW_3_S);
    outportb(PIC_DATA_MASTER, ICW_4);
    outportb(PIC_DATA_SLAVE, ICW_4);
    outportb(PIC_DATA_MASTER, 0xFF);
    outportb(PIC_DATA_SLAVE, 0xFF);
    serial_printf("[APIC](disable_pic) Disabled legacy PIC 8259.\n");
}

void apic_init() {
    disable_pic();
    
    ia32_apic_base_t apic;
    apic.raw = read_msr(IA32_APIC_BASE);
    
    phys_addr_t lapic_phys = (phys_addr_t)(apic.apic_base) << 12;
    virt_addr_t lapic_virt = (virt_addr_t)phys_to_virt(lapic_phys);
    serial_printf("[APIC](apic_init) Local APIC pointer @(PHYS)0x%x @(VIRT)0x%x\n", 
        lapic_phys, lapic_virt
    );
    
    vmm_map_page(kernel_pml4, lapic_virt, lapic_phys, PT_FLAG_PRESENT | PT_FLAG_WRITE);

    // Enable LAPIC
    volatile uint32_t* lapic_regs = (volatile uint32_t*)lapic_virt;
    uint32_t spurious = lapic_regs[SPURIOUS_OFFSET/4];
    spurious &= ~0xFF;
    spurious |= SPURIOUS_VECTOR;
    spurious |= (1 << 8);
    lapic_regs[SPURIOUS_OFFSET/4] = spurious;
}

/* I/O APIC */
void io_apic_write(uint8_t offset, uint32_t value) {
    *(io_apic_base) = offset; // ioregsel
    *(io_apic_base + 4) = value; // iowin
}

uint32_t io_apic_read(uint8_t offset) {
    *(io_apic_base) = offset;
    return *(io_apic_base + 4);
}

// Called from acpi.c
void io_apic_init(phys_addr_t phys_addr) {
    io_apic_base = (volatile uint32_t*)phys_to_virt(phys_addr);

    vmm_map_page(
        kernel_pml4, (virt_addr_t)io_apic_base, phys_addr, 
        PT_FLAG_PRESENT | PT_FLAG_WRITE | PT_FLAG_PCD | PT_FLAG_PWT
    );

    serial_printf("[APIC](io_apic_init) I/O APIC mapped @(VIRT)0x%x\n", io_apic_base);

    uint32_t id = io_apic_read(0x00);
    uint32_t ver = io_apic_read(0x01);
    uint32_t count = (ver >> 16) & 0xFF;
    
    serial_printf("[APIC] I/O APIC ID: %d, Version: 0x%x, Pins: %d\n", 
                  (id >> 24) & 0xF, ver & 0xFF, count + 1);
}