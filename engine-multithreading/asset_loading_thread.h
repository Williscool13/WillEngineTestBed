//
// Created by William on 2025-10-20.
//

#ifndef WILLENGINETESTBED_ASSET_LOADING_THREAD_H
#define WILLENGINETESTBED_ASSET_LOADING_THREAD_H
#include <filesystem>
#include <functional>
#include <thread>


/*struct AssetLoadRequest
{
    std::filesystem::path path;
    std::function<void(ModelHandle)> onComplete;
};

struct AssetLoadComplete
{
    std::function<void(ModelHandle)> onComplete;
};


class AssetLoadingThread
{
public:
    void Start();

    void Stop();

    void RequestLoad(std::filesystem::path path);

    bool PollCompleted(LoadedAsset& out);

private:
    void ThreadLoop();

    std::jthread thread;
    std::atomic<bool> running{false};

    LockFreeQueue<AssetLoadRequest> requestQueue;
    LockFreeQueue<AssetLoadComplete> completeQueue;

    // Vulkan resources for upload
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkFence uploadFence;
    // ...
};

class Engine
{
public:
    Engine()
    {
        assetLoader.Start();
    }

    ~Engine()
    {
        assetLoader.Stop(); // Joins internally
    }

private:
    AssetLoadingThread assetLoader;
};*/

#endif //WILLENGINETESTBED_ASSET_LOADING_THREAD_H
