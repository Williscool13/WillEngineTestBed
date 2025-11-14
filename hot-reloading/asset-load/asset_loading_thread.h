//
// Created by William on 2025-10-20.
//

#ifndef WILLENGINETESTBED_ASSET_LOADING_THREAD_H
#define WILLENGINETESTBED_ASSET_LOADING_THREAD_H
#include <filesystem>
#include <functional>
#include <thread>

#include "asset_load_types.h"
#include "fastgltf/types.hpp"
#include "LockFreeQueue/LockFreeQueueCpp11.h"
#include "core/data-structures/handle_allocator.h"
#include "core/data-structures/ring_buffer.h"
#include "render/render_constants.h"
#include "render/model/model_data.h"


namespace Renderer
{
class ResourceManager;
}

template<typename T>
using LockFreeQueue = LockFreeQueueCpp11<T>;


namespace HotReloading::AssetLoad
{
class AssetLoadingThread
{
public:
    AssetLoadingThread();

    ~AssetLoadingThread();

    void Initialize(Renderer::VulkanContext* context_, Renderer::ResourceManager* resourceManager_);

public: // Threading
    void Start();

    void RequestShutdown();

    void Join();

private: // Threading
    void ThreadMain();

    std::jthread thread;
    std::atomic<bool> running{false};
    LockFreeQueue<AssetLoadRequest> requestQueue{Renderer::ASSET_LOAD_QUEUE_COUNT};
    LockFreeQueue<AssetLoadComplete> completeQueue{Renderer::ASSET_LOAD_QUEUE_COUNT};
    std::vector<AssetLoadInProgress> modelsInProgress{};

public:
    RequestLoad RequestLoad(const std::filesystem::path& path);

    void ResolveLoads(RingBuffer<ModelEntryHandle, MAX_LOADED_MODEL_RING_BUFFER>& loadedModels, std::vector<VkBufferMemoryBarrier2>& bufferBarriers, std::vector<VkImageMemoryBarrier2>& imageBarriers);

    void UnloadModel(ModelEntryHandle handle);

    Renderer::ModelData* GetModelData(ModelEntryHandle handle);

private:
    FreeList<ModelEntry, Renderer::MAX_LOADED_MODELS> models;
    std::unordered_map<std::filesystem::path, ModelEntryHandle> pathToHandle;

private:
    Renderer::VulkanContext* context{};
    Renderer::ResourceManager* resourceManager{};

    std::atomic<bool> bShouldExit{false};

    void CreateDefaultResources();

    ModelEntryHandle LoadGltf(ModelEntryHandle newModelHandle, const std::filesystem::path& path);

    static void UploadTexture(const Renderer::VulkanContext* context, ModelEntry* newModel, const UploadStaging* currentUploadStaging, Renderer::AllocatedImage& image, VkExtent3D extents,
                              uint32_t stagingOffset);

private: // Staging data structures
    uint32_t currentIndex{0};

    VkCommandPool commandPool{};
    std::array<UploadStaging, Renderer::ASSET_LOAD_ASYNC_COUNT> uploadStagingDatas;
    HandleAllocator<UploadStaging, Renderer::ASSET_LOAD_ASYNC_COUNT> uploadStagingHandleAllocator{};
    std::vector<UploadStagingHandle> activeUploadHandles;


    void FinishUploadsInProgress();

    void RemoveFinishedUploadStaging(std::vector<UploadStagingHandle>& uploadStagingHandles);

    void StartUploadStaging(const UploadStaging& uploadStaging);

    UploadStagingHandle GetAvailableStaging();

private: // Texture loading
    void LoadGltfImages(ModelEntry* newModelEntry, UploadStaging*& currentUploadStaging, std::vector<UploadStagingHandle>& uploadStagingHandles, const fastgltf::Asset& asset,
                        const std::filesystem::path& parentFolder);

    ModelEntryHandle defaultResourcesHandle{};
    Renderer::AllocatedImage whiteImage{};
    Renderer::ImageView whiteImageView{};
    int32_t whiteImageDescriptorIndex;
    Renderer::AllocatedImage errorImage{};
    Renderer::ImageView errorImageView{};
    int32_t errorImageDescriptorIndex;
    Renderer::Sampler defaultSamplerLinear{};
    int32_t samplerLinearDescriptorIndex;

private: // Nodes
    std::vector<Renderer::Node> sortedNodes;
    std::vector<bool> visited;

    void TopologicalSortNodes(std::vector<Renderer::Node>& nodes, std::vector<uint32_t>& oldToNew);
};
} // Renderer


#endif //WILLENGINETESTBED_ASSET_LOADING_THREAD_H
