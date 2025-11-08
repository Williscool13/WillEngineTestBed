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
#include "render/render_constants.h"
#include "render/model/model_data.h"
#include "utils/handle_allocator.h"

template<typename T>
using LockFreeQueue = LockFreeQueueCpp11<T>;

namespace Renderer
{
class ResourceManager;


class AssetLoadingThread
{
public:
    AssetLoadingThread();

    ~AssetLoadingThread();

    void Initialize(VulkanContext* context_, ResourceManager* resourceManager_);

public: // Threading
    void Start();

    void RequestShutdown();

    void Join();

private: // Threading
    void ThreadMain();

    std::jthread thread;
    std::atomic<bool> running{false};
    LockFreeQueue<AssetLoadRequest> requestQueue{ASSET_LOAD_QUEUE_COUNT};
    LockFreeQueue<AssetLoadComplete> completeQueue{ASSET_LOAD_QUEUE_COUNT};
    LockFreeQueue<VkBufferMemoryBarrier2> independentBufferAcquireQueue{ASSET_LOAD_INDEPENDENT_BARRIER_COUNT};
    LockFreeQueue<VkImageMemoryBarrier2> independentImageAcquireQueue{ASSET_LOAD_INDEPENDENT_BARRIER_COUNT};

    std::vector<AssetLoadInProgress> modelsInProgress{};

public:
    void RequestLoad(const std::filesystem::path& path, const std::function<void(ModelEntryHandle)>& callback);

    void ResolveLoads(std::vector<ModelEntryHandle>& loadedModelsToAcquire);

    ModelEntryHandle LoadGltf(const std::filesystem::path& path);

    void UnloadModel(ModelEntryHandle handle);

    ModelData* GetModelData(ModelEntryHandle handle);

    AcquireOperations* GetModelAcquires(ModelEntryHandle handle);

private:
    VulkanContext* context{};
    ResourceManager* resourceManager{};

    std::atomic<bool> bShouldExit{false};

    void CreateDefaultResources();

private: // Staging data structures
    uint32_t currentIndex{0};

    VkCommandPool commandPool{};
    std::array<UploadStaging, ASSET_LOAD_ASYNC_COUNT> uploadStagingDatas;
    HandleAllocator<UploadStaging, ASSET_LOAD_ASYNC_COUNT> uploadStagingHandleAllocator{};
    std::vector<UploadStagingHandle> activeUploadHandles;


    void FinishUploadsInProgress();
    void RemoveFinishedUploadStaging(std::vector<UploadStagingHandle>& uploadStagingHandles);
    void StartUploadStaging(const UploadStaging& uploadStaging);
    UploadStagingHandle GetAvailableStaging();

private: // Texture loading
    void LoadGltfImages(ModelEntry* newModelEntry, UploadStaging*& currentUploadStaging, std::vector<UploadStagingHandle>& uploadStagingHandles, const fastgltf::Asset& asset, const std::filesystem::path& parentFolder);

    ModelEntryHandle defaultResourcesHandle{};
    AllocatedImage whiteImage{};
    ImageView whiteImageView{};
    AllocatedImage errorCheckerboardImage{};
    ImageView errorCheckerboardImageView{};
    Sampler defaultSamplerLinear{};

private: // Nodes
    std::vector<Node> sortedNodes;
    std::vector<bool> visited;

    void TopologicalSortNodes(std::vector<Node>& nodes, std::vector<uint32_t>& oldToNew);

private:
    FreeList<ModelEntry, MAX_LOADED_MODELS> models;
    std::unordered_map<std::filesystem::path, ModelEntryHandle> pathToHandle;
};
} // Renderer


#endif //WILLENGINETESTBED_ASSET_LOADING_THREAD_H
