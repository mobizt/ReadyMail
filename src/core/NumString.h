#ifndef NUMSTRING_H
#define NUMSTRING_H

#include <Arduino.h>

class NumString
{
public:
    NumString() {}
    ~NumString() {}
    template <typename T = uint64_t>
    auto get(T val, bool negative = false) -> typename std::enable_if<(std::is_same<T, uint64_t>::value), String>::type
    {
        String v;
        char buffer[21];
        char *ndx = &buffer[sizeof(buffer) - 1];
        *ndx = '\0';
        do
        {
            *--ndx = val % 10 + '0';
            val = val / 10;
        } while (val != 0);

        if (negative)
            v += '-';
        v += ndx;
        return v;
    }

    template <typename T = int64_t>
    auto get(T val) -> typename std::enable_if<(std::is_same<T, int64_t>::value), String>::type
    {
        uint64_t value = val < 0 ? -val : val;
        return get(value, val < 0);
    }

    template <typename T = int>
    auto get(T val) -> typename std::enable_if<(!std::is_same<T, uint64_t>::value && !std::is_same<T, int64_t>::value), String>::type
    {
        return String(val);
    }

    uint32_t toNum(const char *str)
    {
        char *endptr;
        return (uint32_t)strtoul(str, &endptr, 10);
    }
};
#endif