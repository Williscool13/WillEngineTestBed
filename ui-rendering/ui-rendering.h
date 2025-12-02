//
// Created by William on 2025-10-09.
//

#ifndef WILLENGINETESTBED_UIRENDERING_H
#define WILLENGINETESTBED_UIRENDERING_H

#include <memory>
#include <SDL3/SDL.h>

#include "imgui_threaded_rendering.h"
#include "offsetAllocator.hpp"
#include "freetype/freetype.h"
#include "game/camera/free_camera.h"
#include "render/render_constants.h"
#include "render/vk_synchronization.h"
#include "render/vk_resources.h"
#include "render/vk_types.h"
#include "render/descriptor_buffer/descriptor_buffer_bindless_resources.h"
#include "render/descriptor_buffer/descriptor_buffer_combined_image_sampler.h"
#include "render/pipelines/basic_texture_render_pipeline.h"
#include "render/pipelines/render_pipeline.h"
#include "render/pipelines/text_rendering_pipeline.h"
#include "utils/utils.h"


namespace Renderer
{
struct RenderContext;
struct ImguiWrapper;
struct VulkanContext;
struct Swapchain;
struct RenderTargets;
}

namespace UIRendering
{
struct GlyphInfo
{
    int32_t atlasX;
    int32_t atlasY;
    uint32_t width;
    uint32_t height;
    int32_t bearingX;
    int32_t bearingY;
    int32_t advance;
};

class UIRendering
{
public:
    UIRendering();

    ~UIRendering();

    void SetupFont();

    void RenderText(const char* text, float x, float y, float scale, uint32_t color);

    void Initialize();

    void DrawImgui();

    void Run();

    void Render(float deltaTime, uint32_t currentFrameInFlight, Renderer::FrameSynchronization& frameSync, ImDrawDataSnapshot& imguiSnapshot);

    void Cleanup();

private:
    SDL_Window* window{nullptr};
    std::unique_ptr<Renderer::VulkanContext> vulkanContext{};
    std::unique_ptr<Renderer::Swapchain> swapchain{};
    std::unique_ptr<Renderer::ImguiWrapper> imgui{};

    uint64_t frameNumber{0};
    std::unique_ptr<Renderer::RenderContext> renderContext{};
    std::unique_ptr<Renderer::RenderTargets> renderTargets{};
    std::vector<Renderer::FrameSynchronization> frameSynchronization;

    Game::FreeCamera freeCamera{{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f}};
    Renderer::SceneData sceneData{};
    std::vector<Renderer::AllocatedBuffer> sceneDataBuffers;
    std::vector<ImDrawDataSnapshot> imguiFrameBuffers{};


    bool bShouldExit{false};
    bool bSwapchainOutdated{false};

private:
    Renderer::AllocatedBuffer megaVertexBuffer;
    OffsetAllocator::Allocator vertexBufferAllocator{sizeof(Renderer::Vertex) * Renderer::MEGA_VERTEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer megaIndexBuffer;
    OffsetAllocator::Allocator indexBufferAllocator{sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT};

    uint32_t cubeIndexCount{};
    Renderer::RenderPipeline renderPipeline;
    Renderer::BasicTextureRenderPipeline basicTextureRenderPipeline;

    Renderer::TextRenderingPipeline textRenderingPipeline;
    Renderer::DescriptorBufferBindlessResources bindlessResourcesDescriptorBuffer{};
    Renderer::DescriptorSetLayout fontAtlasSetLayout{};
    Renderer::DescriptorBufferCombinedImageSampler fontAtlasDescriptorBuffer{};
    Renderer::AllocatedBuffer textVertexBuffer;
    uint32_t vertexCount{0};
    OffsetAllocator::Allocator textVertexAllocator{sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT};

    VkFence immFence{VK_NULL_HANDLE};
    VkCommandPool immCommandPool{VK_NULL_HANDLE};
    VkCommandBuffer immCommandBuffer{VK_NULL_HANDLE};
    Renderer::AllocatedBuffer imageStagingBuffer{};
    OffsetAllocator::Allocator stagingAllocator{Renderer::STAGING_BUFFER_SIZE};



private:
    FT_Library ft;
    Renderer::AllocatedImage fontAtlas;
    Renderer::ImageView fontAtlasView;
    Renderer::ImageView fontAtlasArrayView;
    Renderer::Sampler defaultSamplerLinear{};
    std::unordered_map<char, GlyphInfo> glyphMap;
};
}


#endif //WILLENGINETESTBED_UIRENDERING_H
