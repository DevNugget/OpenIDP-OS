#ifndef LIBIDP_ANSI_H
#define LIBIDP_ANSI_H

#define ANSI_SET_TITLE(title) "\x1b]0;" title "\x07"

#define ANSI_RESET        "\x1b[0m"
#define ANSI_BOLD         "\x1b[1m"
#define ANSI_DIM          "\x1b[2m"
#define ANSI_ITALIC       "\x1b[3m"
#define ANSI_UNDERLINE    "\x1b[4m"
#define ANSI_BLINK        "\x1b[5m"
#define ANSI_REVERSE      "\x1b[7m"
#define ANSI_HIDDEN       "\x1b[8m"
#define ANSI_STRIKETHROUGH "\x1b[9m"

#define ANSI_FG_BLACK     "\x1b[30m"
#define ANSI_FG_RED       "\x1b[31m"
#define ANSI_FG_GREEN     "\x1b[32m"
#define ANSI_FG_YELLOW    "\x1b[33m"
#define ANSI_FG_BLUE      "\x1b[34m"
#define ANSI_FG_MAGENTA   "\x1b[35m"
#define ANSI_FG_CYAN      "\x1b[36m"
#define ANSI_FG_WHITE     "\x1b[37m"

#define ANSI_FG_BRIGHT_BLACK   "\x1b[90m"
#define ANSI_FG_BRIGHT_RED     "\x1b[91m"
#define ANSI_FG_BRIGHT_GREEN   "\x1b[92m"
#define ANSI_FG_BRIGHT_YELLOW  "\x1b[93m"
#define ANSI_FG_BRIGHT_BLUE    "\x1b[94m"
#define ANSI_FG_BRIGHT_MAGENTA "\x1b[95m"
#define ANSI_FG_BRIGHT_CYAN    "\x1b[96m"
#define ANSI_FG_BRIGHT_WHITE   "\x1b[97m"

#define ANSI_BG_BLACK     "\x1b[40m"
#define ANSI_BG_RED       "\x1b[41m"
#define ANSI_BG_GREEN     "\x1b[42m"
#define ANSI_BG_YELLOW    "\x1b[43m"
#define ANSI_BG_BLUE      "\x1b[44m"
#define ANSI_BG_MAGENTA   "\x1b[45m"
#define ANSI_BG_CYAN      "\x1b[46m"
#define ANSI_BG_WHITE     "\x1b[47m"

#define ANSI_CURSOR_HOME  "\x1b[H"
#define ANSI_CLEAR_SCREEN "\x1b[2J"
#define ANSI_CLEAR_LINE_END "\x1b[K"
#define ANSI_CURSOR_HIDE  "\x1b[?25l"
#define ANSI_CURSOR_SHOW  "\x1b[?25h"

#define ANSI_CURSOR_UP(n)    "\x1b[" #n "A"
#define ANSI_CURSOR_DOWN(n)  "\x1b[" #n "B"
#define ANSI_CURSOR_RIGHT(n) "\x1b[" #n "C"
#define ANSI_CURSOR_LEFT(n)  "\x1b[" #n "D"

#endif //LIBIDP_ANSI_H