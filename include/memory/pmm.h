#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <com1.h>
#include <kstring.h>

#define PAGE_SIZE 4096
#define BITS_PER_BYTE 8

void pmm_init(struct limine_memmap_response* memmap, 
    struct limine_executable_address_response* kernel_addr_request,
    struct limine_hhdm_response* hhdm_response);

void* pmm_alloc_page();
void pmm_free_page(void* addr);

void* pmm_alloc_pages(size_t count);
void pmm_free_pages(void* addr, size_t count);

#endif
