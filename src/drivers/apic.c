#include <drivers/apic.h>
#include <drivers/com1.h>
#include <utility/port.h>
#include <utility/hhdm.h>

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
}

void apic_init() {
    ia32_apic_base_t apic;
    apic.raw = read_msr(IA32_APIC_BASE);
    
    uint64_t lapic = (uint64_t)(apic.apic_base) << 12;
    serial_write_str("[APIC](apic_init) Local APIC pointer physical=0x");
    serial_u64_hex(lapic);
    serial_write_str("\n");
    
    // Enable LAPIC
    uint32_t* lapic_regs = (uint32_t*)phys_to_virt(lapic);
    serial_write_str("[APIC](apic_init) Local APIC pointer virtual=0x");
    serial_u64_hex((uint64_t)lapic_regs);
    serial_write_str("\n");
    uint32_t spurious = lapic_regs[SPURIOUS_OFFSET/4];
    spurious &= ~0xFF;
    spurious |= SPURIOUS_VECTOR;
    spurious |= (1 << 8);
    lapic_regs[SPURIOUS_OFFSET/4] = spurious;
}