//
// Created by William on 2025-10-20.
//

#ifndef WILLENGINETESTBED_ASSET_LOADING_THREAD_H
#define WILLENGINETESTBED_ASSET_LOADING_THREAD_H
#include <filesystem>
#include <functional>
#include <thread>

#include "fastgltf/types.hpp"
#include "LockFreeQueue/LockFreeQueueCpp11.h"
#include "render/render_constants.h"
#include "render/model/model_data.h"

template<typename T>
using LockFreeQueue = LockFreeQueueCpp11<T>;

namespace Renderer
{
class ResourceManager;

struct ModelEntry
{
    enum State { Loading, Ready, Failed };

    ModelData data{};
    uint32_t refCount = 0;
    State state{};
};

struct UploadStaging
{
    VkCommandBuffer commandBuffer{};
    VkFence fence{};

    AllocatedBuffer stagingBuffer{};
    OffsetAllocator::Allocator stagingAllocator{STAGING_SIZE};
};

struct TextureUploadHandle
{
    uint32_t index{0};
    uint32_t generation{0};
};

using ModelEntryHandle = Handle<ModelEntry>;

struct AssetLoadRequest
{
    std::filesystem::path path;
    std::function<void(ModelEntryHandle)> onComplete;
};

struct AssetLoadComplete
{
    ModelEntryHandle handle;
    std::function<void(ModelEntryHandle)> onComplete;
};


class AssetLoadingThread
{
public:
    AssetLoadingThread();

    ~AssetLoadingThread();

    AssetLoadingThread(VulkanContext* context, ResourceManager* resourceManager);

public: // Threading
    void Start();

    void Stop();

private: // Threading
    void ThreadLoop();

    std::jthread thread;
    std::atomic<bool> running{false};
    LockFreeQueue<AssetLoadRequest> requestQueue{ASSET_LOAD_QUEUE_COUNT};
    LockFreeQueue<AssetLoadComplete> completeQueue{ASSET_LOAD_QUEUE_COUNT};

public:
    void RequestLoad(const std::filesystem::path& path, const std::function<void(ModelEntryHandle)>& callback);

    void ResolveLoads();

    ModelEntryHandle LoadGltf(const std::filesystem::path& path);

    void UnloadModel(ModelEntryHandle handle);

private:
    VulkanContext* context{};
    ResourceManager* resourceManager{};

private: // Texture loading
    uint32_t currentIndex{0};
    std::array<UploadStaging, ASSET_LOAD_ASYNC_COUNT> uploadStagingDatas;
    std::array<std::atomic<int32_t>, ASSET_LOAD_ASYNC_COUNT> uploadStagingGenerations{};

    void LoadGltfImages(UploadStaging& uploadStaging, const fastgltf::Asset& asset, const std::filesystem::path& parentFolder, std::vector<AllocatedImage>& outAllocatedImages);

    UploadStaging& GetAvailableTextureStaging();

private: // Nodes
    std::vector<Node> sortedNodes;
    std::vector<bool> visited;

    void TopologicalSortNodes(std::vector<Node>& nodes, std::vector<uint32_t>& oldToNew);

private:
    FreeList<ModelEntry, MAX_LOADED_MODELS> models;
    std::unordered_map<std::filesystem::path, ModelEntryHandle> pathToHandle;
};

// class Engine
// {
// public:
//     Engine()
//     {
//         assetLoader.Start();
//     }
//
//     ~Engine()
//     {
//         assetLoader.Stop(); // Joins internally
//     }
//
// private:
//     AssetLoadingThread assetLoader;
// };
} // Renderer


#endif //WILLENGINETESTBED_ASSET_LOADING_THREAD_H
