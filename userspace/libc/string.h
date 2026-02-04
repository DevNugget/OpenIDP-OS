// string.h - Optimized for x86_64
#ifndef STRING_H
#define STRING_H

#include "stdint.h"

// ----- MEMORY FUNCTIONS -----

static inline void *memcpy(void *dest, const void *src, int n) {
    uint8_t *d = (uint8_t*)dest;
    const uint8_t *s = (const uint8_t*)src;

    // 1. Fast path: 8-byte aligned block copy (Crucial for VRAM)
    if (n >= 8) {
        int q = n / 8;
        asm volatile (
            "rep movsq" 
            : "+D"(d), "+S"(s), "+c"(q) 
            : 
            : "memory"
        );
        n %= 8;
    }

    // 2. Copy remaining bytes
    if (n) {
        asm volatile (
            "rep movsb"
            : "+D"(d), "+S"(s), "+c"(n)
            :
            : "memory"
        );
    }
    return dest;
}

static inline void *memset(void *s, int c, int n) {
    uint8_t *d = (uint8_t*)s;
    uint64_t v = (uint8_t)c;

    // Create 64-bit pattern
    v = v | (v << 8);
    v = v | (v << 16);
    v = v | (v << 32);

    if (n >= 8) {
        int q = n / 8;
        asm volatile (
            "rep stosq"
            : "+D"(d), "+c"(q)
            : "a"(v)
            : "memory"
        );
        n %= 8;
    }

    while (n--) *d++ = (uint8_t)c;
    return s;
}

static inline void *memmove(void *dest, const void *src, int n) {
    uint8_t *d = (uint8_t*)dest;
    const uint8_t *s = (const uint8_t*)src;
    if (d < s) {
        memcpy(dest, src, n);
    } else if (d > s) {
        // Copy backwards
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

static inline int memcmp(const void *s1, const void *s2, int n) {
    const uint8_t *p1 = s1, *p2 = s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

// ----- STRING FUNCTIONS -----

static inline int strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static inline char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

static inline char *strncpy(char *dest, const char *src, int n) {
    int i;
    for (i = 0; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

static inline int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static inline char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == 0) ? (char *)s : NULL;
}

#endif