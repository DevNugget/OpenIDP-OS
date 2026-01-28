#ifndef KHEAP_H
#define KHEAP_H

#include <stdint.h>
#include <stddef.h>
#include <pmm.h>
#include <vmm.h>
#include <com1.h>

#define HEAP_MAGIC 0xDEADBEEFCAFEBABE
#define ALIGNMENT 16
#define MIN_SPLIT 32

#define PAGE_SIZE 4096
#define ALIGN_UP(x, a) (((x) + (a - 1)) & ~(a - 1))

#define KHEAP_START 0xFFFF800000000000UL
#define KHEAP_MAX   0xFFFF800100000000UL  // 256MB heap max for now

void heap_init(void);

void* kmalloc(size_t size);
void kfree(void* ptr);

void* krealloc(void* ptr, size_t size);

#endif