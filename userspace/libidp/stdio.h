#ifndef LIBIDP_STDIO_H
#define LIBIDP_STDIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

void stdio_init(uint64_t stdin_fd, uint64_t stdout_fd);
void stdio_arginit(int* argc, char** argv);

int putchar(int c);
int getchar(void);
int puts(const char* s);
int printf(const char* format, ...);

#endif