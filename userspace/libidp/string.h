#ifndef LIBIDP_STRING_H
#define LIBIDP_STRING_H

#include <stdint.h>
#include <stddef.h>

size_t strlen(const char* s);
size_t strnlen(const char* s, size_t max_len);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
char* strcpy(char* dst, const char* src);
size_t strlcpy(char* dst, const char* src, size_t dst_size);
int starts_with(const char* s, const char* prefix);

int parse_u64(const char* s, uint64_t* out_value);

#endif
