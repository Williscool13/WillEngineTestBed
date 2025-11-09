//
// Created by William on 2025-11-04.
//

#ifndef WILLENGINETESTBED_ASSET_LOAD_TYPES_H
#define WILLENGINETESTBED_ASSET_LOAD_TYPES_H
#include <volk/volk.h>

#include "render/render_constants.h"
#include "render/model/model_data.h"
#include "render/render-operations/render_operations.h"

namespace Renderer
{
struct UploadStaging
{
    VkCommandBuffer commandBuffer{};
    VkFence fence{};

    AllocatedBuffer stagingBuffer{};
    OffsetAllocator::Allocator stagingAllocator{STAGING_BUFFER_SIZE};
};

using UploadStagingHandle = Handle<UploadStaging>;

struct ModelEntry
{
    enum State { Loading, Ready };
    /**
     * When entry is marked as `Ready`, asset thread will not modify the contents of this struct further.
     */
    std::atomic<State> state{};

    // Inserted into by asset loading thread. Used by game thread
    ModelData data{};
    AcquireOperations modelAcquires{};


    // Only accessed by asset loading thread
    uint32_t refCount = 0;
    std::vector<UploadStagingHandle> uploadStagingHandles;
    std::chrono::steady_clock::time_point loadStartTime;
    std::chrono::steady_clock::time_point loadEndTime;

    ModelEntry() = default;

    ModelEntry(ModelEntry&& other) noexcept
        : data(std::move(other.data))
          , refCount(other.refCount)
          , state(other.state.load())
          , uploadStagingHandles(std::move(other.uploadStagingHandles))
    {}

    ModelEntry& operator=(ModelEntry&& other) noexcept
    {
        if (this != &other) {
            data = std::move(other.data);
            refCount = other.refCount;
            state.store(other.state.load());
            uploadStagingHandles = std::move(other.uploadStagingHandles);
        }
        return *this;
    }
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

    bool bNeedToSendToRender{false};
    Transform transform;
    OffsetAllocator::Allocation jointMatrixAllocation{};
    uint32_t jointMatrixOffset{0};
};
} // Renderer

#endif //WILLENGINETESTBED_ASSET_LOAD_TYPES_H
