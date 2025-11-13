//
// Created by William on 2025-11-13.
//

#ifndef WILLENGINETESTBED_RENDER_H
#define WILLENGINETESTBED_RENDER_H
#include "hot-reloading/engine/engine_synchronization.h"
#include "render/descriptor_buffer/descriptor_buffer_storage_image.h"
#include "render/pipelines/draw_cull_compute_pipeline.h"
#include "render/pipelines/main_render_pipeline.h"
#include "render/pipelines/main_skeletal_render_pipeline.h"
#include "render/render-operations/render_operation_ring_buffer.h"
#include "utils/utils.h"

namespace Renderer
{
struct RenderTargets;
struct ImguiWrapper;
struct Swapchain;
class ResourceManager;
struct RenderContext;
struct FrameSynchronization;
}

namespace HotReloading::Render
{
class RenderThread
{
public:
    RenderThread();

    ~RenderThread();

    void Initialize(Engine::EngineSynchronization* engineSync_, SDL_Window* window_, uint32_t w, uint32_t h);

    void CreateBuffers(uint32_t count);

    void InitializeResources();

    void Start();

    void RequestShutdown();

    void Join();

public:
    Renderer::ResourceManager* GetResourceManager() const { return resourceManager.get(); }
    Renderer::VulkanContext* GetVulkanContext() const { return vulkanContext.get(); }

private:
    enum class RenderResponse
    {
        OK,
        SWAPCHAIN_OUTDATED
    };

    void ThreadMain();

    void ProcessAcquisitions(VkCommandBuffer cmd, Renderer::FrameBuffer& currentFrameBuffer);

    void ProcessOperations(uint32_t currentFrameInFlight);

    RenderResponse Render(uint32_t currentRenderFrameInFlight, Renderer::FrameSynchronization& currentFrameSynchronization, Renderer::FrameBuffer& currentFrameBuffer);

private:
    void ConstructSceneData(Renderer::RawSceneData& raw, Renderer::SceneData& scene, float aspectRatio, glm::vec2 renderTargetSize, glm::vec2 texelSize);

private:
    Engine::EngineSynchronization* engineSync{};
    SDL_Window* window{};
    std::unique_ptr<Renderer::VulkanContext> vulkanContext{};
    std::unique_ptr<Renderer::Swapchain> swapchain{};
    std::unique_ptr<Renderer::ImguiWrapper> imgui{};
    std::unique_ptr<Renderer::RenderTargets> renderTargets{};
    std::unique_ptr<Renderer::ResourceManager> resourceManager{};

    bool bSwapchainOutdated{false};
    std::unique_ptr<Renderer::RenderContext> renderContext{};

private:
    Renderer::DescriptorSetLayout renderTargetSetLayout{};
    Renderer::DescriptorBufferStorageImage renderTargetDescriptors{};

    Renderer::DrawCullComputePipeline drawCullComputePipeline{};
    Renderer::MainRenderPipeline mainRenderPipeline{};
    Renderer::MainSkeletalRenderPipeline mainSkeletalRenderPipeline{};

private: // Frame Draw Resources
    uint64_t frameNumber{0};
    std::chrono::steady_clock::time_point lastFrameTime;

    uint32_t renderBufferCount{0};
    std::vector<Renderer::FrameSynchronization> frameSynchronization;

    std::vector<Renderer::AllocatedBuffer> sceneDataBuffers;

    std::vector<Renderer::AllocatedBuffer> modelBuffers;
    std::vector<Renderer::AllocatedBuffer> instanceBuffers;
    std::vector<Renderer::AllocatedBuffer> jointMatrixBuffers;

    uint32_t highestInstanceIndex{0};
    Renderer::AllocatedBuffer opaqueIndexedIndirectBuffer;
    std::vector<Renderer::AllocatedBuffer> indirectCountBuffers;
    Renderer::AllocatedBuffer opaqueSkeletalIndexedIndirectBuffer;
    std::vector<Renderer::AllocatedBuffer> skeletalIndirectCountBuffers;

private:
    Renderer::ModelMatrixOperationRingBuffer modelMatrixOperationRingBuffer;
    Renderer::InstanceOperationRingBuffer instanceOperationRingBuffer;
    Renderer::JointMatrixOperationRingBuffer jointMatrixOperationRingBuffer;

private: // Thread Sync
    std::jthread thread;
    std::atomic<bool> bShouldExit{false};

private: // Performance Tracking
    Utils::FrameTimeTracker frameTimeTracker{1000, 1.5f};

    void UpdateFrameTimeStats(float frameTimeMs);
};

} // HotReloading::Render

#endif //WILLENGINETESTBED_RENDER_H