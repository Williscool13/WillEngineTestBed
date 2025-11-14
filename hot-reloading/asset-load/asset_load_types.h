//
// Created by William on 2025-11-04.
//

#ifndef WILLENGINETESTBED_ASSET_LOAD_TYPES_H
#define WILLENGINETESTBED_ASSET_LOAD_TYPES_H
#include <volk/volk.h>

#include "render/render_constants.h"
#include "render/model/model_data.h"
#include "render/render-operations/render_operations.h"

namespace HotReloading::AssetLoad
{

static inline constexpr int32_t MAX_LOADED_MODEL_RING_BUFFER = 256;

struct UploadStaging
{
    VkCommandBuffer commandBuffer{};
    VkFence fence{};

    Renderer::AllocatedBuffer stagingBuffer{};
    OffsetAllocator::Allocator stagingAllocator{Renderer::STAGING_BUFFER_SIZE};
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
    Renderer::ModelData data{};
    Renderer::AcquireOperations modelAcquires{};


    // Only accessed by asset loading thread
    uint32_t refCount = 0;
    std::vector<UploadStagingHandle> uploadStagingHandles;
    std::chrono::steady_clock::time_point loadStartTime;
    std::chrono::steady_clock::time_point loadEndTime;

    ModelEntry() = default;

    ModelEntry(ModelEntry&& other) noexcept
        : state(other.state.load())
          , data(std::move(other.data))
          , refCount(other.refCount)
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
    ModelEntryHandle modelEntryHandle;
    std::filesystem::path path;
};

struct AssetLoadInProgress
{
    ModelEntryHandle modelEntryHandle;
};

struct AssetLoadComplete
{
    ModelEntryHandle modelEntryHandle;
};

struct RequestLoad
{
    bool bIsLoaded{false};
    ModelEntryHandle modelEntryHandle;
};

struct RuntimeMesh
{
    ModelEntryHandle modelEntryHandle{ModelEntryHandle::Invalid};
    // sorted when generated
    std::vector<Renderer::RuntimeNode> nodes;

    std::vector<uint32_t> nodeRemap{};

    bool bNeedToSendToRender{false};
    Transform transform;
    OffsetAllocator::Allocation jointMatrixAllocation{};
    uint32_t jointMatrixOffset{0};
};
} // HotReloading::AssetLoad

#endif //WILLENGINETESTBED_ASSET_LOAD_TYPES_H
