#include <libidp/type.h>

int is_digit(char c) {
    return c >= '0' && c <= '9';
}

int is_numeric(const char* s) {
    if (!s || s[0] == '\0') return 0;

    for (int i = 0; s[i] != '\0'; ++i) {
        if (!is_digit(s[i])) return 0;
    }

    return 1;
}


