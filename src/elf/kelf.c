#include <kelf.h>

extern uint64_t limine_hhdm;

static int elf_verify_header(const Elf64_Ehdr* header);
static int elf_read_header(FIL* file, Elf64_Ehdr* header);
static Elf64_Phdr* elf_read_program_headers(FIL* file, const Elf64_Ehdr* header);
static uint64_t elf_load_segments(FIL* file, 
                                  uint64_t* pml4_virt, 
                                  const Elf64_Ehdr* header, 
                                  Elf64_Phdr* phdr);

static inline uint64_t get_phys_addr(void* addr) {
    return (uint64_t)addr - limine_hhdm;
}

int load_elf_file(const char* filename, uint64_t* pml4_virt, elf_load_result_t* out) {
    FIL file;

    if (f_open(&file, filename, FA_READ) != FR_OK) {
        serial_printf("Failed to open file: %s\n", filename);
        return -1;
    }

    Elf64_Ehdr header;
    if (!elf_read_header(&file, &header)) {
        f_close(&file);
        return -1;
    }

    Elf64_Phdr* phdrs = elf_read_program_headers(&file, &header);
    if (!phdrs) {
        f_close(&file);
        return -1;
    }

    uint64_t max_vaddr = elf_load_segments(&file, pml4_virt, &header, phdrs);

    kfree(phdrs);
    f_close(&file);

    if (!out) {
        serial_printf("Elf loader error: out parameter is NULL\n");
        return -1;
    }

    out->entry = header.e_entry;
    out->program_break = (max_vaddr + 0xFFF) & ~0xFFF;
    return 1;
}

static int elf_verify_header(const Elf64_Ehdr* header) {
    return (header->e_ident[0] == 0x7F && 
            header->e_ident[1] == 'E'  && 
            header->e_ident[2] == 'L'  && 
            header->e_ident[3] == 'F');
}

static int elf_read_header(FIL* file, Elf64_Ehdr* header) {
    UINT bytes_read;
    FRESULT res = f_read(file, header, sizeof(Elf64_Ehdr), &bytes_read);

    if (res != FR_OK || bytes_read != sizeof(Elf64_Ehdr)) {
        serial_printf("Failed to read ELF header\n");
        f_close(file);
        return 0;
    }

    // Verify ELF magic
    if (!elf_verify_header(header)) {
        serial_printf("Invalid ELF Magic in file\n");
        f_close(file);
        return 0;
    }

    return 1;
}

static Elf64_Phdr* elf_read_program_headers(FIL* file, const Elf64_Ehdr* header) {
    UINT bytes_read;
    uint32_t ph_size = header->e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr* phdr = (Elf64_Phdr*)kmalloc(ph_size);

    if (!phdr) {
        serial_printf("Failed to allocate program headers!\n");
        return NULL;
    }
    
    // Seek to the program header table in the file
    f_lseek(file, header->e_phoff);
    f_read(file, phdr, ph_size, &bytes_read);

    if (bytes_read != ph_size) {
        serial_printf("Failed to read program headers!\n");
        kfree(phdr);
        return NULL;
    }

    return phdr;
}

static uint64_t elf_load_segments(FIL* file, 
                                  uint64_t* pml4_virt, 
                                  const Elf64_Ehdr* header, 
                                  Elf64_Phdr* phdr) {
    UINT bytes_read;

    uint64_t max_vaddr = 0;

    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) { continue; }

        uint64_t filesz = phdr[i].p_filesz;
        uint64_t memsz = phdr[i].p_memsz;
        uint64_t vaddr = phdr[i].p_vaddr;
        uint64_t file_offset = phdr[i].p_offset;

        uint64_t pages_needed = (memsz + 0xFFF) / PAGE_SIZE;
        
        for (uint64_t page_index = 0; page_index < pages_needed; page_index++) {
            uint64_t offset = page_index * PAGE_SIZE;
            
            void* page_virt = pmm_alloc_page();
            uint64_t page_phys = get_phys_addr(page_virt);
            
            vmm_map_page(pml4_virt, 
                         vaddr + offset, 
                         page_phys, 
                         VMM_USER | VMM_WRITE | VMM_PRESENT);
            
            memset(page_virt, 0, PAGE_SIZE);

            if (offset < filesz) {
                uint64_t bytes_to_copy = filesz - offset;
                if (bytes_to_copy > PAGE_SIZE) bytes_to_copy = PAGE_SIZE;
                
                f_lseek(file, file_offset + offset);
                
                f_read(file, page_virt, bytes_to_copy, &bytes_read);
            }
        }

        uint64_t segment_end = phdr[i].p_vaddr + phdr[i].p_memsz;
        if (segment_end > max_vaddr) {
            max_vaddr = segment_end;
        }
    }

    return max_vaddr;
}
