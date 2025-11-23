//
// Created by William on 2025-11-14.
//

#ifndef WILLENGINETESTBED_RING_BUFFER_H
#define WILLENGINETESTBED_RING_BUFFER_H

#include <array>

template<typename T, size_t Capacity>
class RingBuffer
{
public:
    RingBuffer()
        : size(0)
          , head(0)
          , tail(0)
    {}

    bool Push(const T& item)
    {
        if (IsFull()) return false;

        buffer[tail] = item;
        tail = (tail + 1) % Capacity;
        size++;

        return true;
    }

    bool Pop(T& item)
    {
        if (IsEmpty()) return false;

        item = buffer[head];
        head = (head + 1) % Capacity;
        size--;

        return true;
    }

    void Clear()
    {
        head = 0;
        tail = 0;
        size = 0;
    }

    size_t GetSize() const { return size; }
    constexpr size_t GetCapacity() const { return Capacity; }
    bool IsEmpty() const { return size == 0; }
    bool IsFull() const { return size == Capacity; }

private:
    std::array<T, Capacity> buffer;
    size_t size;
    size_t head;
    size_t tail;
};

#endif //WILLENGINETESTBED_RING_BUFFER_H
