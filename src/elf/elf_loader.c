#include <elf/elf_loader.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <utility/align.h>
#include <utility/kstring.h>

#define EI_NIDENT 16
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define ET_EXEC 2
#define ET_DYN 3
#define EM_X86_64 62
#define PT_LOAD 1

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

typedef struct {
    uint8_t e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_header_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_program_header_t;

static int validate_elf64_header(const elf64_header_t* hdr, size_t image_size) {
    if (hdr == NULL || image_size < sizeof(elf64_header_t)) {
        return 0;
    }

    if (hdr->e_ident[0] != 0x7F || hdr->e_ident[1] != 'E' || hdr->e_ident[2] != 'L' || hdr->e_ident[3] != 'F') {
        return 0;
    }

    if (hdr->e_ident[4] != ELFCLASS64 || hdr->e_ident[5] != ELFDATA2LSB || hdr->e_ident[6] != EV_CURRENT) {
        return 0;
    }

    if (hdr->e_machine != EM_X86_64 || (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN)) {
        return 0;
    }

    if (hdr->e_phoff >= image_size) {
        return 0;
    }

    if (hdr->e_phentsize != sizeof(elf64_program_header_t)) {
        return 0;
    }

    uint64_t ph_table_end = hdr->e_phoff + ((uint64_t)hdr->e_phnum * (uint64_t)hdr->e_phentsize);
    if (ph_table_end > image_size) {
        return 0;
    }

    return 1;
}

static uint64_t elf_pflags_to_pt_flags(uint32_t p_flags) {
    uint64_t flags = PT_FLAG_USER;

    if (p_flags & PF_W) {
        flags |= PT_FLAG_WRITE;
    }

    if ((p_flags & PF_X) == 0) {
        flags |= PT_FLAG_NX;
    }

    return flags;
}

int elf64_load_process_image(process_t* process, const void* image, size_t image_size, uint64_t* out_entry_point) {
    if (process == NULL || process->pml4 == NULL || image == NULL || out_entry_point == NULL) {
        return -1;
    }

    const elf64_header_t* hdr = (const elf64_header_t*)image;
    if (!validate_elf64_header(hdr, image_size)) {
        return -1;
    }

    const uint8_t* image_bytes = (const uint8_t*)image;
    const elf64_program_header_t* program_headers = (const elf64_program_header_t*)(image_bytes + hdr->e_phoff);

    phys_addr_t current_cr3 = read_cr3();
    phys_addr_t target_cr3 = (vmm_get_phys(process->pml4) & CR3_ADDR_MASK) | (current_cr3 & ~CR3_ADDR_MASK);

    write_cr3(target_cr3);

    for (uint16_t i = 0; i < hdr->e_phnum; i++) {
        const elf64_program_header_t* phdr = &program_headers[i];

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        if (phdr->p_memsz == 0) {
            continue;
        }

        if (phdr->p_offset > image_size || phdr->p_filesz > image_size || phdr->p_offset + phdr->p_filesz > image_size) {
            write_cr3(current_cr3);
            return -1;
        }

        if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr) {
            write_cr3(current_cr3);
            return -1;
        }

        virt_addr_t seg_start = (virt_addr_t)ALIGN_DOWN(phdr->p_vaddr, PAGE_SIZE);
        virt_addr_t seg_end = (virt_addr_t)ALIGN_UP(phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);
        uint64_t pt_flags = elf_pflags_to_pt_flags(phdr->p_flags);

        for (virt_addr_t page = seg_start; page < seg_end; page += PAGE_SIZE) {
            phys_addr_t phys = pmm_alloc(1);
            if (phys == 0) {
                write_cr3(current_cr3);
                return -1;
            }

            vmm_map_page((phys_addr_t*)process->pml4, page, phys, pt_flags);
            memset((void*)page, 0, PAGE_SIZE);
        }

        memcpy((void*)(uintptr_t)phdr->p_vaddr, image_bytes + phdr->p_offset, phdr->p_filesz);
    }

    write_cr3(current_cr3);

    *out_entry_point = hdr->e_entry;
    return 0;
}
