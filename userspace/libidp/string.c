#include <libidp/string.h>
#include <libidp/type.h>

size_t strlen(const char* s) {
    size_t n = 0;
    if (!s) return 0;
    while (s[n] != '\0') n++;
    return n;
}

size_t strnlen(const char* s, size_t max_len) {
    size_t n = 0;
    if (!s) return 0;
    while (n < max_len && s[n] != '\0') n++;
    return n;
}

int strcmp(const char* a, const char* b) {
    size_t i = 0;
    char ca = 0;
    char cb = 0;

    do {
        ca = (a != 0) ? a[i] : '\0';
        cb = (b != 0) ? b[i] : '\0';
        if (ca != cb) {
            return ((unsigned char)ca < (unsigned char)cb) ? -1 : 1;
        }
        i++;
    } while (ca != '\0');

    return 0;
}

int strncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        char ca = (a != 0) ? a[i] : '\0';
        char cb = (b != 0) ? b[i] : '\0';
        if (ca != cb) {
            return ((unsigned char)ca < (unsigned char)cb) ? -1 : 1;
        }
        if (ca == '\0') return 0;
    }
    return 0;
}

char* strcpy(char* dst, const char* src) {
    if (!dst) return dst;
    if (!src) {
        dst[0] = '\0';
        return dst;
    }

    size_t i = 0;
    while (src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return dst;
}

size_t strlcpy(char* dst, const char* src, size_t dst_size) {
    size_t src_len = strlen(src);

    if (!dst || dst_size == 0) {
        return src_len;
    }

    size_t copy_len = (src_len >= dst_size) ? (dst_size - 1) : src_len;
    for (size_t i = 0; i < copy_len; ++i) {
        dst[i] = src ? src[i] : '\0';
    }
    dst[copy_len] = '\0';
    return src_len;
}

int starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) return 0;
    size_t i = 0;
    while (prefix[i] != '\0') {
        if (s[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

int parse_u64(const char* s, uint64_t* out_value) {
    uint64_t value = 0;

    if (!s || !out_value || s[0] == '\0') {
        return -1;
    }

    for (int i = 0; s[i] != '\0'; ++i) {
        if (!is_digit(s[i])) {
            return -1;
        }
        value = (value * 10u) + (uint64_t)(s[i] - '0');
    }

    *out_value = value;
    return 0;
}