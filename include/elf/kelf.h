#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stddef.h>

#include <vmm.h>
#include <pmm.h>
#include <kheap.h>

#include <fatfs/ff.h>
#include <com1.h>
#include <kstring.h>

#define PT_LOAD 1
#define PF_X 1  // Executable
#define PF_W 2  // Write
#define PF_R 4  // Read

typedef struct {
    unsigned char e_ident[16];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint64_t      e_entry;
    uint64_t      e_phoff;
    uint64_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t   p_type;
    uint32_t   p_flags;
    uint64_t   p_offset;
    uint64_t   p_vaddr;
    uint64_t   p_paddr;
    uint64_t   p_filesz;
    uint64_t   p_memsz;
    uint64_t   p_align;
} Elf64_Phdr;

typedef struct {
    uint64_t entry;
    uint64_t program_break;
} elf_load_result_t;

int load_elf_file(const char* filename, uint64_t* pml4_virt, elf_load_result_t* out);

#endif