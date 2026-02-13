#include <descriptors/gdt.h>

#define GDT_SIZE 5

uint64_t gdt_entries[GDT_SIZE];

void load_gdt(gdtr_t gdtr) {
    __asm__ ("lgdt %0" : : "m"(gdtr));
}

void flush_gdt() {
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

void gdt_init() {
    /* NULL descriptor */
    gdt_entries[0] = 0;
    
    /* KERNEL CODE descriptor */
    uint64_t kernel_code = 0;
    kernel_code |= GDT_TYPE_CODE << 8; 
    kernel_code |= GDT_S_CODE_DATA; 
    kernel_code |= GDT_DPL0; // Ring 0
    kernel_code |= GDT_PRESENT;
    kernel_code |= GDT_LONG_MODE;
    gdt_entries[1] = kernel_code << 32;
    
    /* KERNEL DATA descriptor */
    uint64_t kernel_data = 0;
    kernel_data |= GDT_TYPE_DATA << 8;
    kernel_data |= GDT_S_CODE_DATA; 
    kernel_data |= GDT_DPL0; // Ring 0
    kernel_data |= GDT_PRESENT; 
    kernel_data |= GDT_LONG_MODE; 
    gdt_entries[2] = kernel_data << 32;
    
    /* USER CODE descriptor */
    uint64_t user_code = kernel_code | GDT_DPL3;
    gdt_entries[3] = user_code << 32;
    
    /* USER DATA descriptor */
    uint64_t user_data = kernel_data | GDT_DPL3;
    gdt_entries[4] = user_data << 32;
    
    gdtr_t gdtr;
    gdtr.limit = GDT_SIZE * sizeof(uint64_t) - 1;
    gdtr.address = (uint64_t)gdt_entries;
    
    load_gdt(gdtr);
    flush_gdt();
}