#ifndef LIBIDP_STDIO_H
#define LIBIDP_STDIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

void fflush(void);

int putchar(int c);
int getchar(void);
int puts(const char* s);
int printf(const char* format, ...);

#endif