#ifndef STATIC_VECTOR_H
#define STATIC_VECTOR_H

#include <stddef.h> // for size_t

// Optional macros
#define STATIC_VECTOR_CLEANUP_ENABLED
#define STATIC_VECTOR_DIAGNOSTICS_ENABLED
#define STATIC_VECTOR_OVERFLOW_ABORT

// Minimal type traits
template <typename T>
struct is_trivially_constructible
{
    // static const bool value = __is_trivially_constructible(T);
};

template <typename T>
struct is_trivially_destructible
{
    // static const bool value = __is_trivially_destructible(T);
};

// Embedded-safe move and forward
template <typename T>
constexpr T &&move(T &t) noexcept
{
    return static_cast<T &&>(t);
}

template <typename T>
constexpr T &&forward(T &t) noexcept
{
    return static_cast<T &&>(t);
}

template <typename T, size_t Capacity>
class StaticVector
{
    static_assert(Capacity > 0, "StaticVector capacity must be > 0");
#ifdef STATIC_VECTOR_DIAGNOSTICS_ENABLED
    // static_assert(is_trivially_constructible<T>::value,
    //               "T must be trivially constructible for AVR safety");
#endif

public:
    T data[Capacity];
    size_t count = 0;

   StaticVector(){}

    // Move constructor
    StaticVector(StaticVector &&other) noexcept
    {
        for (size_t i = 0; i < other.count; ++i)
        {
            new (&data[i]) T(move(other.data[i]));
#ifdef STATIC_VECTOR_CLEANUP_ENABLED
            other.data[i].~T();
#endif
        }
        count = other.count;
        other.count = 0;
    }

    // Move assignment
    StaticVector &operator=(StaticVector &&other) noexcept
    {
        if (this != &other)
        {
            clear();
            for (size_t i = 0; i < other.count; ++i)
            {
                new (&data[i]) T(move(other.data[i]));
#ifdef STATIC_VECTOR_CLEANUP_ENABLED
                other.data[i].~T();
#endif
            }
            count = other.count;
            other.count = 0;
        }
        return *this;
    }

    StaticVector &operator=(const StaticVector &other)
    {
        if (this != &other)
        {
            clear(); // Destroy current contents
            for (size_t i = 0; i < other.count; ++i)
            {
                new (&data[i]) T(other.data[i]); // Copy construct
            }
            count = other.count;
        }
        return *this;
    }

    StaticVector(const StaticVector &other)
    {
        for (size_t i = 0; i < other.count; ++i)
        {
            new (&data[i]) T(other.data[i]); // Copy construct each element
        }
        count = other.count;
    }

    constexpr size_t size() const { return count; }

    template <typename... Args>
    bool emplace_back(Args &&...args)
    {
        if (count >= Capacity)
        {
#ifdef STATIC_VECTOR_OVERFLOW_ABORT
            __builtin_trap(); // or while(1); for AVR
#endif
            return false;
        }
        new (&data[count]) T(forward<Args>(args)...);
        ++count;
        return true;
    }

    bool push_back(const T &value)
    {
        if (count >= Capacity)
            return false;
        new (&data[count]) T(value);
        ++count;
        return true;
    }

    bool push_back(T &&value)
    {
        if (count >= Capacity)
            return false;
        new (&data[count]) T(move(value));
        ++count;
        return true;
    }

    bool erase(size_t index)
    {
        if (index >= count)
            return false;
#ifdef STATIC_VECTOR_CLEANUP_ENABLED
        data[index].~T();
#endif
        for (size_t i = index; i < count - 1; ++i)
        {
            new (&data[i]) T(move(data[i + 1]));
#ifdef STATIC_VECTOR_CLEANUP_ENABLED
            data[i + 1].~T();
#endif
        }
        --count;
        return true;
    }

    void clear()
    {
#ifdef STATIC_VECTOR_CLEANUP_ENABLED
        // static_assert(is_trivially_destructible<T>::value,
        //               "T must be trivially destructible for safe clear()");
        for (size_t i = 0; i < count; ++i)
        {
            data[i].~T();
        }
#endif
        count = 0;
    }

    T &operator[](size_t index) { return data[index]; }
    const T &operator[](size_t index) const { return data[index]; }
};

#endif // STATIC_VECTOR_H
