//
// Created by William on 2025-10-31.
//

#ifndef WILLENGINETESTBED_ENGINE_MULTITHREADING_H
#define WILLENGINETESTBED_ENGINE_MULTITHREADING_H


#include <memory>
#include <vector>

#include "offsetAllocator.hpp"
#include "render_thread.h"
#include "SDL3/SDL.h"

#include "render/render_context.h"
#include "render/vk_synchronization.h"
#include "render/vk_resources.h"
#include "render/animation/animation_player.h"
#include "render/descriptor_buffer/descriptor_buffer_bindless_resources.h"
#include "render/descriptor_buffer/descriptor_buffer_storage_image.h"
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
    Renderer::ModelMatrixHandle handle;
    glm::mat4 value;

    // Filled and used by render thread
    uint32_t frames;
};

struct FrameBuffer
{
    Renderer::SceneData sceneData{};
    uint64_t currentFrame{};

    std::vector<ModelMatrixOperation> modelMatrixOperations;
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
    SDL_Window* window{nullptr};


    Renderer::RenderThread renderThread{};

    uint64_t frameNumber{0};
    std::atomic<uint32_t> lastGameFrame{0};

public:
    std::counting_semaphore<Core::FRAMES_IN_FLIGHT> gameFrames{Core::FRAMES_IN_FLIGHT};
    std::counting_semaphore<Core::FRAMES_IN_FLIGHT> renderFrames{0};

    std::array<FrameBuffer, Core::FRAMES_IN_FLIGHT> frameBuffers{};

};


#endif //WILLENGINETESTBED_ENGINE_MULTITHREADING_H
