/* date = February 15th 2026 0:16 pm */
#ifndef KSTRING_H
#define KSTRING_H

#include <stdint.h>
#include <stddef.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

#endif //KSTRING_H