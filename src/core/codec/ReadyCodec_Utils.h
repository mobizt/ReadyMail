#ifndef READY_CODEC_UTILS_H
#define READY_CODEC_UTILS_H

#include <stdlib.h> // for malloc, free
#include <Arduino.h>

// Memory allocation helper
template <typename T>
static inline T rd_mem(size_t count = 1) {
    return reinterpret_cast<T>(malloc(sizeof(std::remove_pointer_t<T>) * count));
}

// Safe cast helper
template <typename T, typename U>
static inline T rd_cast(U *ptr) {
    return reinterpret_cast<T>(ptr);
}

// Free helper
template <typename T>
static inline void rd_free(T *ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = nullptr;
    }
}


static void rd_print_to(String &buff, int size, const char *format, ...)
{
    size += strlen(format) + 10;
    char *s = rd_mem<char *>(size);
    va_list va;
    va_start(va, format);
    vsnprintf(s, size, format, va);
    va_end(va);
    buff += rd_cast<const char *>(s);
    rd_free(&s);
}

#endif // READY_UTILS_H