// string.h - minimal standard string/memory functions for OS
#ifndef STRING_H
#define STRING_H

#include "stdint.h"

// ----- MEMORY FUNCTIONS -----

// Copy memory from src to dest (non-overlapping)
static inline void *memcpy(void *dest, const void *src, int n) {
    uint8_t *d = (uint8_t*)dest;
    const uint8_t *s = (const uint8_t*)src;
    for (int i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

// Copy memory from src to dest (handles overlapping)
static inline void *memmove(void *dest, const void *src, int n) {
    uint8_t *d = (uint8_t*)dest;
    const uint8_t *s = (const uint8_t*)src;
    if (d < s) {
        for (int i = 0; i < n; i++)
            d[i] = s[i];
    } else if (d > s) {
        for (int i = n; i > 0; i--)
            d[i-1] = s[i-1];
    }
    return dest;
}

// Fill memory with a byte
static inline void *memset(void *s, int c, int n) {
    uint8_t *p = (uint8_t*)s;
    for (int i = 0; i < n; i++)
        p[i] = (uint8_t)c;
    return s;
}

// Compare two memory blocks
static inline int memcmp(const void *s1, const void *s2, int n) {
    const uint8_t *a = (const uint8_t*)s1;
    const uint8_t *b = (const uint8_t*)s2;
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (int)a[i] - (int)b[i];
    }
    return 0;
}

// ----- STRING FUNCTIONS -----

// String length (up to '\0')
static inline int strlen(const char *s) {
    int len = 0;
    while (s[len] != '\0') len++;
    return len;
}

// Copy string (dest must be large enough)
static inline char *strcpy(char *dest, const char *src) {
    int i = 0;
    while ((dest[i] = src[i]) != '\0')
        i++;
    return dest;
}

// Copy n characters (pads with '\0' if src shorter)
static inline char *strncpy(char *dest, const char *src, int n) {
    int i = 0;
    for (; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}

// Concatenate strings (dest must be large enough)
static inline char *strcat(char *dest, const char *src) {
    int dlen = strlen(dest);
    int i = 0;
    while ((dest[dlen + i] = src[i]) != '\0')
        i++;
    return dest;
}

// Compare two strings
static inline int strcmp(const char *s1, const char *s2) {
    int i = 0;
    while (s1[i] && s1[i] == s2[i])
        i++;
    return (unsigned char)s1[i] - (unsigned char)s2[i];
}

// Compare up to n characters
static inline int strncmp(const char *s1, const char *s2, int n) {
    int i = 0;
    for (; i < n && s1[i] && s1[i] == s2[i]; i++);
    if (i == n) return 0;
    return (unsigned char)s1[i] - (unsigned char)s2[i];
}

// Find first occurrence of character
static inline char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c)
            return (char*)s;
        s++;
    }
    return (c == 0) ? (char*)s : 0;
}

// Find last occurrence of character
static inline char *strrchr(const char *s, int c) {
    char *last = 0;
    while (*s) {
        if (*s == (char)c)
            last = (char*)s;
        s++;
    }
    return (c == 0) ? (char*)s : last;
}

#endif
