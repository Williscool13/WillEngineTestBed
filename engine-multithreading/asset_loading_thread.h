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
struct ModelEntry
{
    enum State { Loading, Ready, Failed };

    ModelData data{};
    uint32_t refCount = 0;
    State state{};
};

struct TextureUploadStaging
{
    VkCommandBuffer commandBuffer{};
    VkFence fence{};

    AllocatedBuffer stagingBuffer{};
    OffsetAllocator::Allocator stagingAllocator{IMAGE_UPLOAD_STAGING_SIZE};
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
public: // Threading
    void Start();

    void Stop();

private: // Threading
    void ThreadLoop();

    std::jthread thread;
    std::atomic<bool> running{false};
    LockFreeQueue<AssetLoadRequest> requestQueue;
    LockFreeQueue<AssetLoadComplete> completeQueue;

public:
    void RequestLoad(const std::filesystem::path& path, const std::function<void(ModelEntryHandle)>& callback);
    void ResolveLoads();


    ModelEntryHandle LoadGltf(const std::filesystem::path& path);

    void UnloadModel(ModelEntryHandle handle);

private:
    VulkanContext* context{};

private: // Texture loading
    uint32_t currentIndex{0};
    std::array<TextureUploadStaging, ASSET_LOAD_TEXTURE_STAGING_COUNT> textureStagingBuffers;
    std::array<std::atomic<int32_t>, ASSET_LOAD_TEXTURE_STAGING_COUNT> textureStagingGeneration{};

    void LoadGltfImages(const fastgltf::Asset& asset, const std::filesystem::path& parentFolder, std::vector<AllocatedImage>& outAllocatedImages);
    TextureUploadStaging& GetAvailableTextureStaging();

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
