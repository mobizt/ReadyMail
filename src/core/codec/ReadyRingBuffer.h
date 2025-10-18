#ifndef READY_RING_BUFFER_H
#define READY_RING_BUFFER_H

#include <Arduino.h>

template <size_t N>
struct rd_ring_buffer
{
    uint8_t buffer[N];
    volatile size_t head = 0;
    volatile size_t tail = 0;

    inline bool isFull() const
    {
        return ((head + 1) % N) == tail;
    }

    inline bool isEmpty() const
    {
        return head == tail;
    }

    inline size_t available() const
    {
        return (head >= tail) ? (head - tail) : (N - tail + head);
    }

    bool push(uint8_t byte)
    {
        size_t next = (head + 1) % N;
        if (next == tail)
            return false;
        buffer[head] = byte;
        head = next;
        return true;
    }

    bool pop(uint8_t &byte)
    {
        if (isEmpty())
            return false;
        byte = buffer[tail];
        tail = (tail + 1) % N;
        return true;
    }

    void reset()
    {
        head = tail = 0;
    }

    template <typename Buffer>
    void flush_ring_to_print(Buffer &buf, Print &out)
    {
        uint8_t b;
        while (buf.pop(b))
        {
            out.write(b);
        }
    }
};

#endif