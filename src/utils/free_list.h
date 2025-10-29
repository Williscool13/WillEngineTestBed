//
// Created by William on 2025-10-19.
//

#ifndef WILLENGINETESTBED_FREE_LIST_H
#define WILLENGINETESTBED_FREE_LIST_H
#include <array>
#include <vector>


inline static uint32_t INVALID_HANDLE_INDEX = 0xFFFFFF;
inline static uint32_t INVALID_HANDLE_GENERATION = 0xFF;

template<typename T>
struct Handle
{
    uint32_t index: 24;
    uint32_t generation: 8;

    [[nodiscard]] bool IsValid() const { return generation != INVALID_HANDLE_GENERATION; }
    bool operator==(Handle other) const { return index == other.index && generation == other.generation; }

    bool operator<(Handle other) const
    {
        if (index != other.index) return index < other.index;
        return generation < other.generation;
    }

    static const Handle Invalid;
};


template<typename T>
inline const Handle<T> Handle<T>::Invalid{
    INVALID_HANDLE_INDEX,
    INVALID_HANDLE_GENERATION
};

/**
 * Free list data structure that owns the array of T
 * @tparam T
 * @tparam MaxSize maximum allocations of T
 */
template<typename T, size_t MaxSize>
class FreeList
{
    std::vector<T> slots;
    std::vector<uint32_t> generations;

    std::vector<uint32_t> freeIndices;
    uint32_t count = 0;

public:
    FreeList()
    {
        slots.resize(MaxSize);
        generations.resize(MaxSize);
        freeIndices.reserve(MaxSize);
        for (uint32_t i = 0; i < MaxSize; ++i) {
            freeIndices.push_back(MaxSize - 1 - i);
        }
    }

    Handle<T> Add(T data)
    {
        if (freeIndices.empty()) {
            return Handle<T>(INVALID_HANDLE_INDEX, INVALID_HANDLE_GENERATION);
        }
        uint32_t index = freeIndices.back();
        freeIndices.pop_back();
        ++count;

        slots[index] = std::move(data);

        return {index, generations[index]};
    }

    T* Get(Handle<T> handle)
    {
        if (handle.index >= MaxSize) { return nullptr; }
        if (generations[handle.index] != handle.generation) { return nullptr; }
        return &slots[handle.index];
    }

    bool Remove(Handle<T> handle)
    {
        if (auto* item = Get(handle)) {
            *item = T{};

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

    std::array<T, MaxSize>& GetAllItems() { return slots; }
};


#endif //WILLENGINETESTBED_FREE_LIST_H
