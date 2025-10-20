//
// Created by William on 2025-10-20.
//

#ifndef WILLENGINETESTBED_HANDLE_ALLOCATOR_H
#define WILLENGINETESTBED_HANDLE_ALLOCATOR_H

#include <array>
#include <vector>

#include "free_list.h"

template<typename T, size_t MaxSize>
class HandleAllocator
{
    std::array<uint32_t, MaxSize> generations;

    std::vector<uint32_t> freeIndices;
    uint32_t count = 0;

public:
    HandleAllocator()
    {
        freeIndices.reserve(MaxSize);
        for (uint32_t i = 0; i < MaxSize; ++i) {
            freeIndices.push_back(MaxSize - 1 - i);
            generations[i] = 1;
        }
    }

    Handle<T> Add()
    {
        if (freeIndices.empty()) {
            return Handle<T>(INVALID_HANDLE_INDEX, INVALID_HANDLE_GENERATION);
        }
        uint32_t index = freeIndices.back();
        freeIndices.pop_back();

        ++count;

        return {index, generations[index]};
    }


    bool Remove(Handle<T> handle)
    {
        if (auto* item = Get(handle)) {
            ++generations[handle.index];
            freeIndices.push_back(handle.index);
            --count;
            return true;
        }

        return false;
    }

    void Clear()
    {
        freeIndices.clear();
        for (uint32_t i = 0; i < MaxSize; ++i) {
            freeIndices.push_back(MaxSize - 1 - i);
            ++generations[i]; // invalidate all existing handles
        }
        count = 0;
    }

    bool IsValid(Handle<T> handle)
    {
        if (handle.index >= MaxSize) { return false; }
        if (generations[handle.index] != handle.generation) { return false; }
        return true;
    }
};

#endif //WILLENGINETESTBED_HANDLE_ALLOCATOR_H
