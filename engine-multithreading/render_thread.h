//
// Created by William on 2025-10-31.
//

#ifndef WILLENGINETESTBED_RENDER_THREAD_H
#define WILLENGINETESTBED_RENDER_THREAD_H
#include <array>
#include <memory>
#include <semaphore>
#include <thread>

#include <SDL3/SDL.h>

#include "core/constants.h"
#include "render/render_constants.h"
#include "render/vk_resources.h"
#include "render/vk_synchronization.h"
#include "render/vk_types.h"
#include "render/descriptor_buffer/descriptor_buffer_bindless_resources.h"
#include "render/descriptor_buffer/descriptor_buffer_storage_image.h"
#include "render/pipelines/gradient_compute_pipeline.h"
#include "utils/utils.h"

class EngineMultithreading;

namespace Renderer
{
struct RenderContext;
class ModelLoader;
struct RenderTargets;
struct ImguiWrapper;
struct Swapchain;
struct VulkanContext;



class RenderThread
{
public:
    RenderThread();

    ~RenderThread();

    void Initialize(EngineMultithreading* engineMultithreading_, SDL_Window* window_, uint32_t w, uint32_t h);

    void InitializeResources();

    void Start();

    void RequestShutdown();

    void Join();

private:
    enum class RenderResponse
    {
        OK,
        SWAPCHAIN_OUTDATED
    };

    void ThreadMain();

    RenderResponse Render(uint32_t currentFrameInFlight, FrameSynchronization& currentFrameData);

private:
    EngineMultithreading* engineMultithreading{};
    SDL_Window* window{};
    std::unique_ptr<VulkanContext> vulkanContext{};
    std::unique_ptr<Swapchain> swapchain{};
    std::unique_ptr<ImguiWrapper> imgui{};
    std::unique_ptr<RenderTargets> renderTargets{};
    //std::unique_ptr<ModelLoader> modelLoader{};

    bool bSwapchainOutdated{false};
    std::unique_ptr<RenderContext> renderContext{};

    uint64_t frameNumber{0};
    std::array<FrameSynchronization, Core::FRAMES_IN_FLIGHT> frameSynchronization;

    SceneData sceneData{};
    std::array<AllocatedBuffer, Core::FRAMES_IN_FLIGHT> sceneDataBuffers;

private:
    DescriptorSetLayout renderTargetSetLayout{};
    DescriptorBufferStorageImage renderTargetDescriptors{};

    DescriptorBufferBindlessResources bindlessResourcesDescriptorBuffer{};

    GradientComputePipeline gradientComputePipeline{};

private: // Thread Sync
    std::jthread thread;
    std::atomic<bool> bShouldExit{false};

private: // Performance Tracking
    Utils::FrameTimeTracker frameTimeTracker{100, 1.5f};

    void UpdateFrameTimeStats(float frameTimeMs);
};
} // Renderer

#endif //WILLENGINETESTBED_RENDER_THREAD_H
