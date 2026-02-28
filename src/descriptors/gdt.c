#include <descriptors/gdt.h>
#include <utility/kstring.h>

#define GDT_MAX_CPUS 64
#define GDT_ENTRY_COUNT 7
#define TSS_STACK_SIZE (16 * 1024)

static uint64_t gdt_tables[GDT_MAX_CPUS][GDT_ENTRY_COUNT];
static tss64_t tss_tables[GDT_MAX_CPUS];
static uint8_t tss_stacks[GDT_MAX_CPUS][TSS_STACK_SIZE];

static inline void load_gdt(gdtr_t gdtr) {
    __asm__ volatile ("lgdt %0" : : "m"(gdtr));
}

static inline void load_tr(uint16_t selector) {
    __asm__ volatile ("ltr %0" : : "r"(selector));
}

static inline void flush_segments(void) {
    __asm__ __volatile__ 
    (
     "mov $0x10, %%ax \n"
     "mov %%ax, %%ds \n"
     "mov %%ax, %%es \n"
     "mov %%ax, %%fs \n"
     "mov %%ax, %%gs \n"
     "mov %%ax, %%ss \n"
     
     "pushq $0x08 \n"     
     "leaq 1f(%%rip), %%rax \n"      
     "pushq %%rax \n"               
     "lretq \n"                    
     "1: \n"                       
     : 
     : 
     : "rax", "memory"            
     );
}

static uint64_t make_code_or_data_descriptor(uint64_t flags) {
    return flags << 32;
}

static void install_tss_descriptor(uint64_t* gdt, const tss64_t* tss) {
    uintptr_t base = (uintptr_t)tss;
    uint64_t limit = sizeof(tss64_t) - 1;

    uint64_t low = 0;
    low |= (limit & 0xFFFFULL);
    low |= (base & 0xFFFFFFULL) << 16;
    low |= (uint64_t)0x9 << 40;      // available 64-bit TSS
    low |= (uint64_t)1 << 47;        // present
    low |= ((limit >> 16) & 0xFULL) << 48;
    low |= ((base >> 24) & 0xFFULL) << 56;

    uint64_t high = (base >> 32) & 0xFFFFFFFFULL;

    gdt[5] = low;
    gdt[6] = high;
}

static void init_cpu_tables(size_t cpu_index) {
    uint64_t* gdt = gdt_tables[cpu_index];
    tss64_t* tss = &tss_tables[cpu_index];

    memset(gdt, 0, sizeof(uint64_t) * GDT_ENTRY_COUNT);
    memset(tss, 0, sizeof(tss64_t));

    uint64_t kernel_code = 0;
    kernel_code |= GDT_TYPE_CODE << 8;
    kernel_code |= GDT_S_CODE_DATA;
    kernel_code |= GDT_DPL0;
    kernel_code |= GDT_PRESENT;
    kernel_code |= GDT_LONG_MODE;

    uint64_t kernel_data = 0;
    kernel_data |= GDT_TYPE_DATA << 8;
    kernel_data |= GDT_S_CODE_DATA;
    kernel_data |= GDT_DPL0;
    kernel_data |= GDT_PRESENT;
    kernel_data |= GDT_LONG_MODE;

    uint64_t user_code = kernel_code | GDT_DPL3;
    uint64_t user_data = kernel_data | GDT_DPL3;

    gdt[1] = make_code_or_data_descriptor(kernel_code);
    gdt[2] = make_code_or_data_descriptor(kernel_data);
    gdt[3] = make_code_or_data_descriptor(user_code);
    gdt[4] = make_code_or_data_descriptor(user_data);

    tss->rsp0 = (uint64_t)&tss_stacks[cpu_index][0] + TSS_STACK_SIZE;
    tss->ist1 = tss->rsp0;

    tss->iomap_base = sizeof(tss64_t);
    install_tss_descriptor(gdt, tss);
}

void gdt_init_cpu(size_t cpu_index) {
    if (cpu_index >= GDT_MAX_CPUS) {
        cpu_index = 0;
    }

    init_cpu_tables(cpu_index);

    gdtr_t gdtr;
    gdtr.limit = (uint16_t)(sizeof(uint64_t) * GDT_ENTRY_COUNT - 1);
    gdtr.address = (uint64_t)&gdt_tables[cpu_index][0];

    load_gdt(gdtr);
    flush_segments();
    load_tr(TSS_SELECTOR);
}

void gdt_init(void) {
    gdt_init_cpu(0);
}