//
// Created by William on 2025-10-31.
//

#ifndef WILLENGINETESTBED_RENDER_THREAD_H
#define WILLENGINETESTBED_RENDER_THREAD_H

#include <memory>
#include <semaphore>
#include <thread>

#include <SDL3/SDL.h>

#include "imgui_threaded_rendering.h"
#include "render/render-operations/render_operations.h"
#include "render/vk_resources.h"
#include "render/vk_synchronization.h"
#include "render/vk_types.h"
#include "render/descriptor_buffer/descriptor_buffer_bindless_resources.h"
#include "render/descriptor_buffer/descriptor_buffer_storage_image.h"
#include "render/model/model_data.h"
#include "render/pipelines/draw_cull_compute_pipeline.h"
#include "render/pipelines/main_render_pipeline.h"
#include "render/pipelines/main_skeletal_render_pipeline.h"
#include "render/render-operations/render_operation_ring_buffer.h"
#include "utils/utils.h"

class EngineMultithreading;

namespace Renderer
{
class ResourceManager;
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

    void CreateBuffers(uint32_t count);

    void InitializeResources();

    void Start();

    void RequestShutdown();

    void Join();

public:
    ResourceManager* GetResourceManager() const { return resourceManager.get(); }
    VulkanContext* GetVulkanContext() const { return vulkanContext.get(); }
    Swapchain* GetSwapchain() const { return swapchain.get(); }

private:
    enum class RenderResponse
    {
        OK,
        SWAPCHAIN_OUTDATED
    };

    void ThreadMain();

    void ProcessAcquisitions(VkCommandBuffer cmd, FrameBuffer& currentFrameBuffer);

    void ProcessOperations(uint32_t currentFrameInFlight);

    RenderResponse Render(uint32_t currentRenderFrameInFlight, FrameSynchronization& currentFrameSynchronization, FrameBuffer& currentFrameBuffer, ImDrawDataSnapshot& currentImguiSnapshot);

private:
    void ConstructSceneData(RawSceneData& raw, SceneData& scene, float aspectRatio, glm::vec2 renderTargetSize, glm::vec2 texelSize);

private:
    EngineMultithreading* engineMultithreading{};
    SDL_Window* window{};
    std::unique_ptr<VulkanContext> vulkanContext{};
    std::unique_ptr<Swapchain> swapchain{};
    std::unique_ptr<RenderTargets> renderTargets{};
    std::unique_ptr<ResourceManager> resourceManager{};

    bool bSwapchainOutdated{false};
    std::unique_ptr<RenderContext> renderContext{};

private:
    DescriptorSetLayout renderTargetSetLayout{};
    DescriptorBufferStorageImage renderTargetDescriptors{};

    DrawCullComputePipeline drawCullComputePipeline{};
    MainRenderPipeline mainRenderPipeline{};
    MainSkeletalRenderPipeline mainSkeletalRenderPipeline{};

private: // Frame Draw Resources
    uint64_t frameNumber{0};
    std::chrono::steady_clock::time_point lastFrameTime;

    uint32_t renderBufferCount{0};
    std::vector<FrameSynchronization> frameSynchronization;

    std::vector<AllocatedBuffer> sceneDataBuffers;

    std::vector<AllocatedBuffer> modelBuffers;
    std::vector<AllocatedBuffer> instanceBuffers;
    std::vector<AllocatedBuffer> jointMatrixBuffers;

    uint32_t highestInstanceIndex{0};
    AllocatedBuffer opaqueIndexedIndirectBuffer;
    std::vector<AllocatedBuffer> indirectCountBuffers;
    AllocatedBuffer opaqueSkeletalIndexedIndirectBuffer;
    std::vector<AllocatedBuffer> skeletalIndirectCountBuffers;

private:
    ModelMatrixOperationRingBuffer modelMatrixOperationRingBuffer;
    InstanceOperationRingBuffer instanceOperationRingBuffer;
    JointMatrixOperationRingBuffer jointMatrixOperationRingBuffer;

private: // Thread Sync
    std::jthread thread;
    std::atomic<bool> bShouldExit{false};

private: // Performance Tracking
    Utils::FrameTimeTracker frameTimeTracker{1000, 1.5f};

    void UpdateFrameTimeStats(float frameTimeMs);
};
} // Renderer

#endif //WILLENGINETESTBED_RENDER_THREAD_H
