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
#include "game/camera/free_camera.h"
#include "render/render_operations.h"
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

class EngineMultithreading
{
public:
    EngineMultithreading();

    ~EngineMultithreading();

    void Initialize();

    void Run();

    void ThreadMain();

    void Cleanup();

private:
    Renderer::RuntimeMesh GenerateModel(Renderer::ModelEntryHandle modelEntryHandle, const Transform& topLevelTransform);

    void UpdateTransforms(std::vector<Renderer::RuntimeNode>& runtimeNodes, const Transform& topLevelTransform);

    void InitialUploadRuntimeMesh(Renderer::RuntimeMesh& runtimeMesh,
                                  std::vector<Renderer::ModelMatrixOperation>& modelMatrixOperations,
                                  std::vector<Renderer::InstanceOperation>& instanceOperations,
                                  std::vector<Renderer::JointMatrixOperation>& jointMatrixOperations);

    void UpdateRuntimeMesh(Renderer::RuntimeMesh& runtimeMesh,
                           std::vector<Renderer::ModelMatrixOperation>& modelMatrixOperations,
                           std::vector<Renderer::InstanceOperation>& instanceOperations,
                           std::vector<Renderer::JointMatrixOperation>& jointMatrixOperations);

    void DeleteRuntimeMesh(Renderer::RuntimeMesh& runtimeMesh,
                           std::vector<Renderer::ModelMatrixOperation>& modelMatrixOperations,
                           std::vector<Renderer::InstanceOperation>& instanceOperations,
                           std::vector<Renderer::JointMatrixOperation>& jointMatrixOperations);

private:
    Renderer::ModelEntryHandle suzanneModelEntryHandle{Renderer::ModelEntryHandle::Invalid};
    Renderer::RuntimeMesh suzanneRuntimeMesh{};
    Renderer::ModelEntryHandle structureModelEntryHandle{Renderer::ModelEntryHandle::Invalid};
    Renderer::RuntimeMesh structureRuntimeMesh{};
    Renderer::ModelEntryHandle riggedFigureModelEntryHandle{Renderer::ModelEntryHandle::Invalid};
    Renderer::RuntimeMesh riggedFigureRuntimeMesh{};
    Renderer::ModelEntryHandle texturedBoxModelEntryHandle{Renderer::ModelEntryHandle::Invalid};
    Renderer::RuntimeMesh texturedBoxRuntimeMesh{};

private:
    SDL_Window* window{nullptr};

    Renderer::RenderThread renderThread{};
    Renderer::AssetLoadingThread assetLoadingThread{};

    uint64_t frameNumber{0};
    Renderer::RawSceneData rawSceneData;
    Game::FreeCamera freeCamera;

public:
    std::counting_semaphore<Core::FRAMES_IN_FLIGHT> gameFrames{Core::FRAMES_IN_FLIGHT};
    std::counting_semaphore<Core::FRAMES_IN_FLIGHT> renderFrames{0};

    std::array<Renderer::FrameBuffer, Core::FRAMES_IN_FLIGHT> frameBuffers{};

private:
    std::chrono::time_point<std::chrono::steady_clock> start{};
};


#endif //WILLENGINETESTBED_ENGINE_MULTITHREADING_H
