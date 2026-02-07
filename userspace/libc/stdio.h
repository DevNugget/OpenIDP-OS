#ifndef IDP_STDIO_H
#define IDP_STDIO_H

#define EOF (-1)

extern int terminal_pid;

void stdio_init(void); // Wait for terminal connection
void putchar(char c);
char getchar(void);
void printf(const char* fmt, ...);
void clear_screen();

#endif
