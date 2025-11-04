//
// Created by William on 2025-10-31.
//

#ifndef WILLENGINETESTBED_ENGINE_MULTITHREADING_H
#define WILLENGINETESTBED_ENGINE_MULTITHREADING_H


#include <memory>
#include <vector>

#include "asset_loading_thread.h"
#include "offsetAllocator.hpp"
#include "render_thread.h"
#include "SDL3/SDL.h"

#include "render/vk_resources.h"
#include "render/animation/animation_player.h"
#include "render/model/model_data.h"
#include "utils/handle_allocator.h"

namespace Renderer
{
class ModelLoader;
struct DescriptorBufferStorageImage;
struct DescriptorBufferCombinedImageSampler;
struct DescriptorBufferUniform;
struct ImguiWrapper;
struct VulkanContext;
struct Swapchain;
struct RenderTargets;
}

struct ModelMatrixOperation
{
    uint32_t index;
    glm::mat4 modelMatrix;

    // Filled and used by render thread
    uint32_t frames;
};

struct InstanceOperation
{
    uint32_t index;
    Renderer::Instance instance;

    // Filled and used by render thread
    uint32_t frames;
};

struct JointMatrixOperation
{
    uint32_t index;
    glm::mat4 jointMatrix;

    // Filled and used by render thread
    uint32_t frames;
};

struct FrameBuffer
{
    Renderer::SceneData sceneData{};
    uint64_t currentFrame{};

    // todo: make ring buffer
    std::vector<ModelMatrixOperation> modelMatrixOperations;
    std::vector<InstanceOperation> instanceOperations;
};

class EngineMultithreading
{
public:
    EngineMultithreading();

    ~EngineMultithreading();

    void Initialize();

    void Run();

    void Cleanup();

private:
    Renderer::RuntimeMesh GenerateModel(Renderer::ModelEntryHandle modelEntryHandle, const Transform& topLevelTransform);

    void UpdateTransforms(std::vector<Renderer::RuntimeNode>& runtimeNodes, const Transform& topLevelTransform);
    void InitialUploadRuntimeMesh(Renderer::RuntimeMesh& runtimeMesh,
                           std::vector<ModelMatrixOperation>& modelMatrixOperations,
                           std::vector<InstanceOperation>& instanceOperations,
                           std::vector<JointMatrixOperation>& jointMatrixOperations);
    void UpdateRuntimeMesh(Renderer::RuntimeMesh& runtimeMesh,
                           std::vector<ModelMatrixOperation>& modelMatrixOperations,
                           std::vector<InstanceOperation>& instanceOperations,
                           std::vector<JointMatrixOperation>& jointMatrixOperations);
    void DeleteRuntimeMesh(Renderer::RuntimeMesh& runtimeMesh,
                           std::vector<ModelMatrixOperation>& modelMatrixOperations,
                           std::vector<InstanceOperation>& instanceOperations,
                           std::vector<JointMatrixOperation>& jointMatrixOperations);

private:
    SDL_Window* window{nullptr};


    Renderer::RenderThread renderThread{};
    Renderer::AssetLoadingThread assetLoadingThread{};

    uint64_t frameNumber{0};
    std::atomic<uint32_t> lastGameFrame{0};

public:
    std::counting_semaphore<Core::FRAMES_IN_FLIGHT> gameFrames{Core::FRAMES_IN_FLIGHT};
    std::counting_semaphore<Core::FRAMES_IN_FLIGHT> renderFrames{0};

    std::array<FrameBuffer, Core::FRAMES_IN_FLIGHT> frameBuffers{};
};


#endif //WILLENGINETESTBED_ENGINE_MULTITHREADING_H
