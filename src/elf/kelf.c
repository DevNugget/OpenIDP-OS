#include <kelf.h>

// Returns the Entry Point (RIP)
uint64_t load_elf(uint64_t* pml4, void* elf_data) {
    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_data;

    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' || 
        header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        return 0; // Not an ELF
    }

    Elf64_Phdr* phdr = (Elf64_Phdr*)(elf_data + header->e_phoff);

    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            // We need to map pages for this segment
            uint64_t vaddr = phdr[i].p_vaddr;
            uint64_t memsz = phdr[i].p_memsz;
            uint64_t filesz = phdr[i].p_filesz;
            uint64_t file_offset = phdr[i].p_offset;
            
            // Calculate how many pages we need
            uint64_t num_pages = (memsz + 0xFFF) / 4096;
            
            // Allocate and map these pages in the USER PML4
            for (uint64_t j = 0; j < num_pages; j++) {
                void* phys = pmm_alloc_page();
                // Map with USER + WRITE + PRESENT flags (0x07)
                vmm_map_page(pml4, vaddr + (j * 4096), (uint64_t)phys, 0x07);
            }

            // Now we need to copy the data.
            // PROBLEM: We are currently in Kernel PML4, we can't write to 'vaddr' directly 
            // if it belongs to the user PML4 we are building.
            // FIX: Temporarily switch CR3 or map the physical pages to a temporary kernel address.
            // (Simplest for now: Switch CR3, copy, Switch back)
            
            uint64_t old_cr3 = read_cr3();
            write_cr3((uint64_t)pml4); // Switch to new process memory
            
            memset((void*)vaddr, 0, memsz); // Zero out BSS
            memcpy((void*)vaddr, elf_data + file_offset, filesz); // Copy code/data
            
            write_cr3(old_cr3); // Switch back
        }
    }
    return header->e_entry;
}