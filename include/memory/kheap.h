/* date = February 17th 2026 9:32 pm */

#ifndef KHEAP_H
#define KHEAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <utility/hhdm.h>

void kheap_init();
void* kmalloc(size_t size);
void kfree(void* ptr);

#endif //KHEAP_H
