//
// Created by William on 2025-11-04.
//

#ifndef WILLENGINETESTBED_ASSET_LOAD_TYPES_H
#define WILLENGINETESTBED_ASSET_LOAD_TYPES_H
#include <volk/volk.h>

#include "render/render_constants.h"
#include "render/model/model_data.h"

namespace Renderer
{
struct UploadStagingHandle
{
    uint32_t index{0};
    uint32_t generation{0};
};

struct ModelEntry
{
    enum State { Loading, Ready };

    ModelData data{};
    uint32_t refCount = 0;
    std::atomic<State> state{};

    // Used by asset thread to find out if a model is ready.
    //todo: use multiple upload staging handles if the upload requires more staging buffer space.
    UploadStagingHandle uploadStagingHandle;

    std::chrono::steady_clock::time_point loadStartTime;
    std::chrono::steady_clock::time_point loadEndTime;

    ModelEntry() = default;

    ModelEntry(ModelEntry&& other) noexcept
        : data(std::move(other.data))
          , refCount(other.refCount)
          , state(other.state.load())
          , uploadStagingHandle(std::move(other.uploadStagingHandle))
    {}

    ModelEntry& operator=(ModelEntry&& other) noexcept
    {
        if (this != &other) {
            data = std::move(other.data);
            refCount = other.refCount;
            state.store(other.state.load());
            uploadStagingHandle = std::move(other.uploadStagingHandle);
        }
        return *this;
    }
};

struct UploadStaging
{
    VkCommandBuffer commandBuffer{};
    VkFence fence{};

    AllocatedBuffer stagingBuffer{};
    OffsetAllocator::Allocator stagingAllocator{STAGING_BUFFER_SIZE};
};

using ModelEntryHandle = Handle<ModelEntry>;

struct AssetLoadRequest
{
    std::filesystem::path path;
    std::function<void(ModelEntryHandle)> onComplete;
};

struct AssetLoadInProgress
{
    ModelEntryHandle modelEntryHandle;
    std::function<void(ModelEntryHandle)> onComplete;
};

struct AssetLoadComplete
{
    ModelEntryHandle handle;
    std::function<void(ModelEntryHandle)> onComplete;
};

struct RuntimeMesh
{
    ModelEntryHandle modelEntryHandle{ModelEntryHandle::Invalid};
    // sorted when generated
    std::vector<RuntimeNode> nodes;

    std::vector<uint32_t> nodeRemap{};

    Transform transform;
    OffsetAllocator::Allocation jointMatrixAllocation{};
    uint32_t jointMatrixOffset{0};
};
} // Renderer

#endif //WILLENGINETESTBED_ASSET_LOAD_TYPES_H
