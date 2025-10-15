#ifndef DYNAMIC_VECTOR_H
#define DYNAMIC_VECTOR_H

#include <stdlib.h>
#include <stddef.h>
#include <type_traits>

// Optional macros
#define DYNAMIC_VECTOR_HEAP_ENABLED
#define DYNAMIC_VECTOR_DIAGNOSTICS_ENABLED

template <typename T>
class DynamicVector
{
    T *data = nullptr;
    size_t count = 0;
    size_t capacity = 0;

public:
    explicit DynamicVector(size_t initial_capacity = 4)
        : capacity(initial_capacity)
    {
#ifdef DYNAMIC_VECTOR_HEAP_ENABLED
        data = static_cast<T *>(malloc(sizeof(T) * capacity));
#endif
    }

    ~DynamicVector()
    {
        clear();
#ifdef DYNAMIC_VECTOR_HEAP_ENABLED
        free(data);
#endif
    }

    size_t size() const { return count; }

    template <typename... Args>
    bool emplace_back(Args &&...args)
    {
        if (count >= capacity)
        {
#ifdef DYNAMIC_VECTOR_HEAP_ENABLED
            if (!reserve(capacity * 2))
            {
#ifdef DYNAMIC_VECTOR_DIAGNOSTICS_ENABLED
                __builtin_trap();
#endif
                return false;
            }
#else
#ifdef DYNAMIC_VECTOR_DIAGNOSTICS_ENABLED
            __builtin_trap();
#endif
            return false;
#endif
        }
        new (&data[count]) T(static_cast<Args &&>(args)...);
        ++count;
        return true;
    }

    bool push_back(const T &value)
    {
        return emplace_back(value);
    }

    bool push_back(T &&value)
    {
        return emplace_back(move(value));
    }

    bool erase(size_t index)
    {
        if (index >= count)
            return false;
        data[index].~T();
        for (size_t i = index; i < count - 1; ++i)
        {
            new (&data[i]) T(move(data[i + 1]));
            data[i + 1].~T();
        }
        --count;
        return true;
    }

    void clear()
    {
        for (size_t i = 0; i < count; ++i)
        {
            data[i].~T();
        }
        count = 0;
    }

    bool reserve(size_t new_capacity)
    {
#ifdef DYNAMIC_VECTOR_HEAP_ENABLED
        if (new_capacity <= capacity)
            return true;

        T *new_data = static_cast<T *>(malloc(sizeof(T) * new_capacity));
        if (!new_data)
            return false;

        for (size_t i = 0; i < count; ++i)
        {
            new (&new_data[i]) T(move(data[i])); // Move construct
            data[i].~T();                        // Destroy old
        }

        free(data);
        data = new_data;
        capacity = new_capacity;
        return true;
#else
        return false;
#endif
    }

    bool shrink_to_fit()
    {
#ifdef DYNAMIC_VECTOR_HEAP_ENABLED
        if (count == capacity)
            return true;
        T *new_data = static_cast<T *>(realloc(data, sizeof(T) * count));
        if (!new_data)
            return false;
        data = new_data;
        capacity = count;
        return true;
#else
        return false;
#endif
    }

    T &operator[](size_t index) { return data[index]; }
    const T &operator[](size_t index) const { return data[index]; }
};

#endif // DYNAMIC_VECTOR_H