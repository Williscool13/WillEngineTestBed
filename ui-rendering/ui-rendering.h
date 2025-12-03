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
#include "render/pipelines/ui_text_rendering_pipeline.h"
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

struct UIRect
{
    glm::vec2 pos;
    glm::vec2 size;
    glm::vec4 color;
};

struct UIImage
{
    glm::vec2 pos;
    glm::vec2 size;
    uint32_t bindlessSamplerIndex;
    uint32_t bindlessTextureIndex;
    glm::vec4 tint;
};

struct UIButton
{
    UIRect rect;
    std::function<void()> onClick;
    bool isHovered;
};

struct UIButtonImage
{
    UIImage rect;
    std::function<void()> onClick;
    bool isHovered;
};

struct UIText
{
    std::string text;
    glm::vec2 pos;
    uint32_t packedColor;
    uint32_t fontIndex;
    float scale;
};

struct UIState
{
    std::vector<UIRect> rects;
    std::vector<UIImage> images;
    std::vector<UIButton> buttons;
    std::vector<UIButtonImage> buttonImages;
    std::vector<UIText> texts;

    void Clear()
    {
        rects.clear();
        images.clear();
        buttons.clear();
        buttonImages.clear();
        texts.clear();
    }
};

class UIRendering
{
public:
    UIRendering();

    ~UIRendering();

    void SetupFont();

    void Initialize();

    void DrawImgui();

    void Run();

    void Render(float deltaTime, uint32_t currentFrameInFlight, Renderer::FrameSynchronization& frameSync, ImDrawDataSnapshot& imguiSnapshot);

    void Cleanup();

    void GenerateUIBuffer(Renderer::AllocatedBuffer& buffer, uint32_t& vertexCount);

    static void UIRenderRect(UIRect& rect, Renderer::AllocatedBuffer& buffer, uint32_t& vertexCount);
    static void UIRenderImage(UIImage& image, Renderer::AllocatedBuffer& buffer, uint32_t& vertexCount);
    static void UIRenderText(UIText& text, std::unordered_map<char, GlyphInfo>& glyphMap, Renderer::UIVertex* vertices, int32_t& vertexIndex);

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

    Renderer::UITextRenderingPipeline textRenderingPipeline;
    Renderer::DescriptorBufferBindlessResources bindlessResourcesDescriptorBuffer{};
    Renderer::DescriptorSetLayout fontAtlasSetLayout{};
    Renderer::DescriptorBufferCombinedImageSampler fontAtlasDescriptorBuffer{};
    // buffer, vertexCount
    std::vector<std::pair<Renderer::AllocatedBuffer, uint32_t> > textVertexBuffers;
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

    Renderer::AllocatedImage whiteTexture;
    Renderer::ImageView whiteTextureView;

    UIState uiState{};
};
}


#endif //WILLENGINETESTBED_UIRENDERING_H
