//
// Created by William on 2025-12-01.
//

#include "ui-rendering.h"

#include "VkBootstrap.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include "core/time.h"

#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"
#include "freetype/freetype.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"

#include "render/vk_context.h"
#include "render/vk_swapchain.h"
#include "render/vk_helpers.h"
#include "render/render_utils.h"
#include "render/render_constants.h"
#include "render/render_targets.h"

#include "input/input.h"
#include "render/render_context.h"
#include "render/vk_descriptors.h"
#include "render/vk_imgui_wrapper.h"
#include "render/animation/animation_player.h"
#include "render/animation/animation_player.h"
#include "render/animation/animation_player.h"
#include "render/animation/animation_player.h"
#include "render/animation/animation_player.h"
#include "render/animation/animation_player.h"

namespace UIRendering
{
UIRendering::UIRendering() = default;

UIRendering::~UIRendering() = default;

void UIRendering::SetupFont()
{
    if (FT_Init_FreeType(&ft)) {
        LOG_ERROR("Failed to init freetype");
        return;
    }

    FT_Face face;
    if (FT_New_Face(ft, "../assets/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf", 0, &face)) {
        LOG_ERROR("Failed to open Roboto font");
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);

    constexpr std::array alphabets{
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
        ' ', '!'
    };

    constexpr VkExtent3D fontExtents{Renderer::FONT_ATLAS_DIM, Renderer::FONT_ATLAS_DIM, 1};
    VkImageCreateInfo imageCreateInfo = Renderer::VkHelpers::ImageCreateInfo(
        VK_FORMAT_R8_UNORM, fontExtents,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    imageCreateInfo.arrayLayers = 4;
    fontAtlas = Renderer::VkResources::CreateAllocatedImage(vulkanContext.get(), imageCreateInfo);


    VK_CHECK(vkResetFences(vulkanContext->device, 1, &immFence));
    VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));
    const VkCommandBufferBeginInfo cmdBeginInfo = Renderer::VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(immCommandBuffer, &cmdBeginInfo));

    VkImageMemoryBarrier2 barrier = Renderer::VkHelpers::ImageMemoryBarrier(
        fontAtlas.handle,
        Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(immCommandBuffer, &depInfo);

    std::vector<VkBufferImageCopy> copies{};
    stagingAllocator.reset();
    uint16_t currentX = 0;
    uint16_t currentY = 0;
    int rowHeight = 0;
    for (char upperLetter : alphabets) {
        if (FT_Load_Char(face, upperLetter, FT_LOAD_RENDER)) {
            LOG_ERROR("Failed to load {}", upperLetter);
            continue;
        }

        FT_GlyphSlot slot = face->glyph;
        FT_Bitmap& bitmap = slot->bitmap;
        size_t size = bitmap.width * bitmap.rows;

        if (currentX + bitmap.width > Renderer::FONT_ATLAS_DIM) {
            currentX = 0;
            currentY += rowHeight;
            rowHeight = 0;
        }

        OffsetAllocator::Allocation bitmapAlloc = stagingAllocator.allocate(size);
        char* bufferOffset = static_cast<char*>(imageStagingBuffer.allocationInfo.pMappedData) + bitmapAlloc.offset;
        memcpy(bufferOffset, bitmap.buffer, size);

        glyphMap[upperLetter] = {
            .atlasX = currentX,
            .atlasY = currentY,
            .width = static_cast<uint16_t>(bitmap.width),
            .height = static_cast<uint16_t>(bitmap.rows),
            .bearingX = static_cast<int16_t>(slot->bitmap_left),
            .bearingY = static_cast<int16_t>(slot->bitmap_top),
            .advance = static_cast<uint16_t>(slot->advance.x >> 6),
        };

        if (bitmap.width != 0 && bitmap.rows != 0) {
            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = bitmapAlloc.offset;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageOffset = {currentX, currentY, 0};
            copyRegion.imageExtent = {bitmap.width, bitmap.rows, 1};
            copies.push_back(copyRegion);
        }

        currentX += bitmap.width;
        rowHeight = std::max(rowHeight, static_cast<int>(bitmap.rows));
    }

    vkCmdCopyBufferToImage(immCommandBuffer, imageStagingBuffer.handle, fontAtlas.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copies.size(), copies.data());

    barrier = Renderer::VkHelpers::ImageMemoryBarrier(
        fontAtlas.handle,
        Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(immCommandBuffer, &depInfo);


    VK_CHECK(vkEndCommandBuffer(immCommandBuffer));
    VkCommandBufferSubmitInfo cmdSubmitInfo = Renderer::VkHelpers::CommandBufferSubmitInfo(immCommandBuffer);
    const VkSubmitInfo2 submitInfo = Renderer::VkHelpers::SubmitInfo(&cmdSubmitInfo, nullptr, nullptr);
    VK_CHECK(vkQueueSubmit2(vulkanContext->graphicsQueue, 1, &submitInfo, immFence));
    vkWaitForFences(vulkanContext->device, 1, &immFence, true, UINT64_MAX);

    // =============== White Image ===============
    VK_CHECK(vkResetFences(vulkanContext->device, 1, &immFence));
    VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));
    VK_CHECK(vkBeginCommandBuffer(immCommandBuffer, &cmdBeginInfo));

    stagingAllocator.reset();

    constexpr VkExtent3D whiteTextureExtent{1, 1, 1};
    VkImageCreateInfo whiteTextureImageCreateInfo = Renderer::VkHelpers::ImageCreateInfo(
        VK_FORMAT_R8G8B8A8_UNORM, whiteTextureExtent,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    whiteTexture = Renderer::VkResources::CreateAllocatedImage(vulkanContext.get(), whiteTextureImageCreateInfo);

    const uint32_t white = packUnorm4x8(glm::vec4(1, 1, 1, 1));
    OffsetAllocator::Allocation whiteAlloc = stagingAllocator.allocate(sizeof(uint32_t));
    char* whiteBufferOffset = static_cast<char*>(imageStagingBuffer.allocationInfo.pMappedData) + whiteAlloc.offset;
    memcpy(whiteBufferOffset, &white, 4);

    barrier = Renderer::VkHelpers::ImageMemoryBarrier(
        whiteTexture.handle,
        Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    vkCmdPipelineBarrier2(immCommandBuffer, &depInfo);

    VkBufferImageCopy whiteCopy = {};
    whiteCopy.bufferOffset = whiteAlloc.offset;
    whiteCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    whiteCopy.imageSubresource.layerCount = 1;
    whiteCopy.imageExtent = {1, 1, 1};
    vkCmdCopyBufferToImage(immCommandBuffer, imageStagingBuffer.handle, whiteTexture.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &whiteCopy);

    barrier = Renderer::VkHelpers::ImageMemoryBarrier(
        whiteTexture.handle,
        Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
        VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    vkCmdPipelineBarrier2(immCommandBuffer, &depInfo);

    VK_CHECK(vkEndCommandBuffer(immCommandBuffer));
    VK_CHECK(vkQueueSubmit2(vulkanContext->graphicsQueue, 1, &submitInfo, immFence));
    vkWaitForFences(vulkanContext->device, 1, &immFence, true, UINT64_MAX);
    // FT_Get_Kerning()

    VkImageViewCreateInfo imageViewCreateInfo = Renderer::VkHelpers::ImageViewCreateInfo(fontAtlas.handle, fontAtlas.format, VK_IMAGE_ASPECT_COLOR_BIT);
    fontAtlasView = Renderer::VkResources::CreateImageView(vulkanContext.get(), imageViewCreateInfo);

    imageViewCreateInfo = Renderer::VkHelpers::ImageViewCreateInfo(whiteTexture.handle, whiteTexture.format, VK_IMAGE_ASPECT_COLOR_BIT);
    whiteTextureView = Renderer::VkResources::CreateImageView(vulkanContext.get(), imageViewCreateInfo);


    VkDescriptorImageInfo imageInfo{
        .imageView = whiteTextureView.handle,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

    };
    whiteTextureIndex = bindlessResourcesDescriptorBuffer.AllocateTexture(imageInfo);

    imageInfo.imageView = fontAtlasView.handle;;
    atlasTextureIndex = bindlessResourcesDescriptorBuffer.AllocateTexture(imageInfo);

    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 16.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = VK_LOD_CLAMP_NONE,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    defaultSamplerLinear = Renderer::VkResources::CreateSampler(vulkanContext.get(), samplerInfo);
    defaultSamplerIndex = bindlessResourcesDescriptorBuffer.AllocateSampler(defaultSamplerLinear.handle);

    imageViewCreateInfo = Renderer::VkHelpers::ImageViewCreateInfo(
        fontAtlas.handle,
        fontAtlas.format,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    imageViewCreateInfo.subresourceRange.layerCount = 4;
    fontAtlasArrayView = Renderer::VkResources::CreateImageView(vulkanContext.get(), imageViewCreateInfo);
    VkDescriptorImageInfo imageInfo2{
        .sampler = defaultSamplerLinear.handle,
        .imageView = fontAtlasArrayView.handle,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

    };
    int32_t index2 = fontAtlasDescriptorBuffer.AllocateDescriptorSet();
    fontAtlasDescriptorBuffer.UpdateDescriptor(imageInfo2, index2, 0, 0);
}

void UIRendering::Initialize()
{
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    renderContext = std::make_unique<Renderer::RenderContext>(Renderer::DEFAULT_SWAPCHAIN_WIDTH, Renderer::DEFAULT_SWAPCHAIN_HEIGHT, Renderer::DEFAULT_RENDER_SCALE);
    std::array<uint32_t, 2> renderExtent = renderContext->GetRenderExtent();
    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(
        "Template",
        renderExtent[0],
        renderExtent[1],
        window_flags);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    int32_t w;
    int32_t h;
    SDL_GetWindowSize(window, &w, &h);
    Input::Get().Init(window, w, h);

    renderContext->RequestRenderExtentResize(w, h);
    renderContext->ApplyRenderExtentResize();

    vulkanContext = std::make_unique<Renderer::VulkanContext>(window);
    swapchain = std::make_unique<Renderer::Swapchain>(vulkanContext.get(), w, h);
    renderTargets = std::make_unique<Renderer::RenderTargets>(vulkanContext.get(), w, h);
    imgui = std::make_unique<Renderer::ImguiWrapper>(vulkanContext.get(), window, swapchain->imageCount, swapchain->format);

    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Renderer::SceneData);

    // make 3 even though we may only get 2 from swapchain (optimize for production, simplify for test)
    constexpr int32_t tripleBuffering = 3;
    frameSynchronization.reserve(tripleBuffering);
    for (int32_t i = 0; i < tripleBuffering; ++i) {
        frameSynchronization.emplace_back(vulkanContext.get());
        frameSynchronization[i].Initialize();

        sceneDataBuffers.push_back(Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));
        imguiFrameBuffers.emplace_back();
    }

    bufferInfo.usage = VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT;
    bufferInfo.size = sizeof(Renderer::Vertex) * Renderer::MEGA_VERTEX_BUFFER_COUNT;
    megaVertexBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    for (int32_t i = 0; i < tripleBuffering; ++i) {
        bufferInfo.size = sizeof(Renderer::UIVertex) * Renderer::MEGA_VERTEX_BUFFER_COUNT;
        textVertexBuffers.emplace_back(Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo), 0);
    }

    bufferInfo.usage = VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT;
    bufferInfo.size = sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT;
    megaIndexBuffer = Renderer::VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);


    DescriptorLayoutBuilder layoutBuilder{2};
    layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.Build(
        static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
    );
    fontAtlasSetLayout = Renderer::VkResources::CreateDescriptorSetLayout(vulkanContext.get(), layoutCreateInfo);

    bindlessResourcesDescriptorBuffer = Renderer::DescriptorBufferBindlessResources(vulkanContext.get());
    fontAtlasDescriptorBuffer = Renderer::DescriptorBufferCombinedImageSampler(vulkanContext.get(), fontAtlasSetLayout.handle);

    renderPipeline = Renderer::RenderPipeline(vulkanContext.get());
    basicTextureRenderPipeline = Renderer::BasicTextureRenderPipeline(vulkanContext.get(), bindlessResourcesDescriptorBuffer.descriptorSetLayout.handle);
    textRenderingPipeline = Renderer::UITextRenderingPipeline(vulkanContext.get(), fontAtlasSetLayout.handle, bindlessResourcesDescriptorBuffer.descriptorSetLayout.handle);

    // setup basic cube
    std::vector<Renderer::Vertex> cubeVertices = {
        // Front face (z+)
        {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        // Back face (z-)
        {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        // Top face (y+)
        {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
        {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        // Bottom face (y-)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        // Right face (x+)
        {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        // Left face (x-)
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
    };

    std::vector<uint32_t> cubeIndices = {
        0, 1, 2, 2, 3, 0, // Front
        4, 5, 6, 6, 7, 4, // Back
        8, 9, 10, 10, 11, 8, // Top
        12, 13, 14, 14, 15, 12, // Bottom
        16, 17, 18, 18, 19, 16, // Right
        20, 21, 22, 22, 23, 20 // Left
    };

    void* vertexDataPtr = megaVertexBuffer.allocationInfo.pMappedData;
    memcpy(vertexDataPtr, cubeVertices.data(), cubeVertices.size() * sizeof(Renderer::Vertex));

    void* indexDataPtr = megaIndexBuffer.allocationInfo.pMappedData;
    memcpy(indexDataPtr, cubeIndices.data(), cubeIndices.size() * sizeof(uint32_t));

    cubeIndexCount = static_cast<uint32_t>(cubeIndices.size());


    const VkFenceCreateInfo fenceInfo = Renderer::VkHelpers::FenceCreateInfo();
    VK_CHECK(vkCreateFence(vulkanContext->device, &fenceInfo, nullptr, &immFence));

    const VkCommandPoolCreateInfo poolInfo = Renderer::VkHelpers::CommandPoolCreateInfo(vulkanContext->graphicsQueueFamily);
    VK_CHECK(vkCreateCommandPool(vulkanContext->device, &poolInfo, nullptr, &immCommandPool));

    const VkCommandBufferAllocateInfo allocInfo = Renderer::VkHelpers::CommandBufferAllocateInfo(1, immCommandPool);
    VK_CHECK(vkAllocateCommandBuffers(vulkanContext->device, &allocInfo, &immCommandBuffer));

    imageStagingBuffer = Renderer::VkResources::CreateAllocatedStagingBuffer(vulkanContext.get(), Renderer::STAGING_BUFFER_SIZE);

    SetupFont();
}

void UIRendering::DrawImgui()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (ImGui::Begin("Main")) {
        ImGui::Text("Hello!");
        float camPos[3];
        camPos[0] = freeCamera.transform.translation.x;
        camPos[1] = freeCamera.transform.translation.y;
        camPos[2] = freeCamera.transform.translation.z;
        if (ImGui::DragFloat3("Position", camPos)) {
            freeCamera.transform.translation = {camPos[0], camPos[1], camPos[2]};
        }
    }

    ImGui::End();
    ImGui::Render();
}

void UIRendering::Run()
{
    Input& input = Input::Input::Get();
    Time& time = Time::Get();

    SDL_Event e;
    bool exit = false;
    while (true) {
        if (input.IsKeyPressed(Key::RETURN)) {
            Uint32 flags = SDL_GetWindowFlags(window);
            if (flags & SDL_WINDOW_BORDERLESS) {
                SDL_SetWindowFullscreen(window, 0);
                SDL_SetWindowBordered(window, true);
                bSwapchainOutdated = true;
            }
            else {
                SDL_SetWindowBordered(window, false);
                SDL_SetWindowFullscreen(window, true);
                bSwapchainOutdated = true;
            }
        }

        input.FrameReset();
        while (SDL_PollEvent(&e) != 0) {
            input.ProcessEvent(e);
            imgui->HandleInput(e);
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
            if (e.type == SDL_EVENT_WINDOW_MINIMIZED
                || e.type == SDL_EVENT_WINDOW_RESTORED
                || e.type == SDL_EVENT_WINDOW_MAXIMIZED
                || e.type == SDL_EVENT_WINDOW_RESIZED
                || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                bSwapchainOutdated = true;
            }
        }

        if (exit) {
            bShouldExit = true;
            break;
        }

        if (bSwapchainOutdated) {
            vkDeviceWaitIdle(vulkanContext->device);

            int32_t w, h;
            SDL_GetWindowSize(window, &w, &h);

            swapchain->Recreate(w, h);
            Input::Input::Get().UpdateWindowExtent(swapchain->extent.width, swapchain->extent.height);
            if (Renderer::RENDER_TARGET_SIZE_EQUALS_SWAPCHAIN_SIZE) { renderContext->RequestRenderExtentResize(w, h); }

            for (Renderer::FrameSynchronization& frameSync : frameSynchronization) {
                frameSync.RecreateSynchronization();
            }

            bSwapchainOutdated = false;
        }

        if (renderContext->HasPendingRenderExtentChanges()) {
            vkDeviceWaitIdle(vulkanContext->device);
            renderContext->ApplyRenderExtentResize();

            std::array<uint32_t, 2> newExtents = renderContext->GetRenderExtent();
            renderTargets->Recreate(newExtents[0], newExtents[1]);
        }

        input.UpdateFocus(SDL_GetWindowFlags(window));
        time.Update();

        const float deltaTime = Time::Get().GetDeltaTime();
        freeCamera.Update(deltaTime);

        const uint32_t currentFrameInFlight = frameNumber % swapchain->imageCount;

        Renderer::FrameSynchronization& currentFrameSync = frameSynchronization[currentFrameInFlight];
        ImDrawDataSnapshot& currentImguiFrameBuffer = imguiFrameBuffers[currentFrameInFlight];

        DrawImgui();
        currentImguiFrameBuffer.SnapUsingSwap(ImGui::GetDrawData(), ImGui::GetTime());

        Render(deltaTime, currentFrameInFlight, currentFrameSync, currentImguiFrameBuffer);
        frameNumber++;
    }
}

void UIRendering::Render(float deltaTime, uint32_t currentFrameInFlight, Renderer::FrameSynchronization& frameSync, ImDrawDataSnapshot& imguiSnapshot)
{
    VK_CHECK(vkWaitForFences(vulkanContext->device, 1, &frameSync.renderFence, true, UINT64_MAX));
    VK_CHECK(vkResetFences(vulkanContext->device, 1, &frameSync.renderFence));

    std::array<uint32_t, 2> scaledRenderExtent = renderContext->GetScaledRenderExtent();

    std::pair<Renderer::AllocatedBuffer, uint32_t>& currentUIBuffer = textVertexBuffers[currentFrameInFlight];
    GenerateUIBuffer(currentUIBuffer.first, currentUIBuffer.second);

    //
    {
        const glm::vec3 cameraPos = freeCamera.GetPosition();
        const glm::vec3 forward = freeCamera.GetForward();
        const glm::vec3 up = freeCamera.GetUp();

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + forward, up);

        glm::mat4 proj = glm::perspective(
            freeCamera.GetFov(),
            static_cast<float>(scaledRenderExtent[0]) / static_cast<float>(scaledRenderExtent[1]),
            freeCamera.GetFarPlane(),
            freeCamera.GetNearPlane()
        );

        sceneData.view = view;
        sceneData.proj = proj;
        sceneData.viewProj = proj * view;
        sceneData.renderTargetSize.x = scaledRenderExtent[0];
        sceneData.renderTargetSize.y = scaledRenderExtent[1];
        sceneData.deltaTime = deltaTime;

        Renderer::AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];
        auto currentSceneData = static_cast<Renderer::SceneData*>(currentSceneDataBuffer.allocationInfo.pMappedData);
        *currentSceneData = sceneData;
    }

    VkCommandBuffer cmd = frameSync.commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo commandBufferBeginInfo = Renderer::VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));

    //
    {
        auto subresource = Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = Renderer::VkHelpers::ImageMemoryBarrier(
            renderTargets->drawImage.handle,
            subresource,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );
        auto dependencyInfo = Renderer::VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }

    constexpr VkClearValue colorClear = {.color = {0.3f, 0.0f, 0.0f, 1.0f}};
    const VkRenderingAttachmentInfo colorAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->drawImageView.handle, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};
    const VkRenderingAttachmentInfo depthAttachment = Renderer::VkHelpers::RenderingAttachmentInfo(renderTargets->depthImageView.handle, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    const VkRenderingInfo renderInfo = Renderer::VkHelpers::RenderingInfo({scaledRenderExtent[0], scaledRenderExtent[1]}, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);
    //
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, basicTextureRenderPipeline.pipeline.handle);

        VkViewport viewport = Renderer::VkHelpers::GenerateViewport(scaledRenderExtent[0], scaledRenderExtent[1]);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor = Renderer::VkHelpers::GenerateScissor(scaledRenderExtent[0], scaledRenderExtent[1]);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        Renderer::AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];
        Renderer::Render3PushConstants pushData{
            glm::mat4(1.0f),
            currentSceneDataBuffer.address,
            0,
            0,
        };

        vkCmdPushConstants(cmd, basicTextureRenderPipeline.pipelineLayout.handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::Render3PushConstants), &pushData);

        VkDescriptorBufferBindingInfoEXT bindingInfo = bindlessResourcesDescriptorBuffer.GetBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 1, &bindingInfo);
        uint32_t bufferIndexImage = 0;
        VkDeviceSize bufferOffset = 0;
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, basicTextureRenderPipeline.pipelineLayout.handle, 0, 1, &bufferIndexImage, &bufferOffset);

        constexpr VkDeviceSize vertexOffset{0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &megaVertexBuffer.handle, &vertexOffset);
        vkCmdBindIndexBuffer(cmd, megaIndexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, cubeIndexCount, 1, 0, 0, 0);
    }

    //
    if (currentUIBuffer.second > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, textRenderingPipeline.pipeline.handle);

        VkViewport viewport = Renderer::VkHelpers::GenerateViewport(scaledRenderExtent[0], scaledRenderExtent[1]);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor = Renderer::VkHelpers::GenerateScissor(scaledRenderExtent[0], scaledRenderExtent[1]);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        Renderer::AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];
        Renderer::TextRenderingPushConstant pushData{
            currentSceneDataBuffer.address,
        };

        vkCmdPushConstants(cmd, textRenderingPipeline.pipelineLayout.handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::TextRenderingPushConstant), &pushData);
        VkDescriptorBufferBindingInfoEXT bindingInfos[2];
        bindingInfos[0] = fontAtlasDescriptorBuffer.GetBindingInfo();
        bindingInfos[1] = bindlessResourcesDescriptorBuffer.GetBindingInfo();
        vkCmdBindDescriptorBuffersEXT(cmd, 2, bindingInfos);

        uint32_t bufferIndices[2] = {0, 1};
        VkDeviceSize bufferOffsets[2] = {0, 0};
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, textRenderingPipeline.pipelineLayout.handle, 0, 2, bufferIndices, bufferOffsets);

        constexpr VkDeviceSize vertexOffset{0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &currentUIBuffer.first.handle, &vertexOffset);
        vkCmdDraw(cmd, currentUIBuffer.second, 1, 0, 0);
    }

    vkCmdEndRendering(cmd);

    uint32_t swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(vulkanContext->device, swapchain->handle, UINT64_MAX, frameSync.swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        bSwapchainOutdated = true;
        LOG_WARN("Swapchain out of date or suboptimal (Acquire)");
        return;
    }
    VkImage currentSwapchainImage = swapchain->swapchainImages[swapchainImageIndex];


    // todo: barriers
    // Imgui Draw
    {
        // VkRenderingAttachmentInfo imguiAttachment{};
        // imguiAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        // imguiAttachment.pNext = nullptr;
        // imguiAttachment.imageView = renderTargets->drawImageView.handle;
        // imguiAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // imguiAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        // imguiAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // VkRenderingInfo renderInfo{};
        // renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        // renderInfo.pNext = nullptr;
        // renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, swapchain->extent};
        // renderInfo.layerCount = 1;
        // renderInfo.colorAttachmentCount = 1;
        // renderInfo.pColorAttachments = &imguiAttachment;
        // renderInfo.pDepthAttachment = nullptr;
        // renderInfo.pStencilAttachment = nullptr;
        //
        // vkCmdBeginRendering(cmd, &renderInfo);
        // ImGui_ImplVulkan_RenderDrawData(&imguiSnapshot.DrawData, cmd);
        // vkCmdEndRendering(cmd);
    }


    // Prepare for copy
    {
        VkImageMemoryBarrier2 barriers[2];
        barriers[0] = Renderer::VkHelpers::ImageMemoryBarrier(
            renderTargets->drawImage.handle,
            Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        );
        barriers[1] = Renderer::VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        depInfo.imageMemoryBarrierCount = 2;
        depInfo.pImageMemoryBarriers = barriers;
        vkCmdPipelineBarrier2(cmd, &depInfo);
    }

    // Blit
    {
        VkOffset3D renderOffset = {static_cast<int32_t>(scaledRenderExtent[0]), static_cast<int32_t>(scaledRenderExtent[1]), 1};
        VkOffset3D swapchainOffset = {static_cast<int32_t>(swapchain->extent.width), static_cast<int32_t>(swapchain->extent.height), 1};
        VkImageBlit2 blitRegion{};
        blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.layerCount = 1;
        blitRegion.srcOffsets[0] = {0, 0, 0};
        blitRegion.srcOffsets[1] = renderOffset;
        blitRegion.dstOffsets[0] = {0, 0, 0};
        blitRegion.dstOffsets[1] = swapchainOffset;

        VkBlitImageInfo2 blitInfo{};
        blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        blitInfo.srcImage = renderTargets->drawImage.handle;
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.dstImage = currentSwapchainImage;
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blitRegion;
        blitInfo.filter = VK_FILTER_LINEAR;

        vkCmdBlitImage2(cmd, &blitInfo);
    }

    //
    {
        auto subresource = Renderer::VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = Renderer::VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            subresource,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );
        auto dependencyInfo = Renderer::VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }


    VK_CHECK(vkEndCommandBuffer(cmd));


    VkCommandBufferSubmitInfo commandBufferSubmitInfo = Renderer::VkHelpers::CommandBufferSubmitInfo(frameSync.commandBuffer);
    VkSemaphoreSubmitInfo swapchainSemaphoreWaitInfo = Renderer::VkHelpers::SemaphoreSubmitInfo(frameSync.swapchainSemaphore, VK_PIPELINE_STAGE_2_BLIT_BIT);
    VkSemaphoreSubmitInfo renderSemaphoreSignalInfo = Renderer::VkHelpers::SemaphoreSubmitInfo(frameSync.renderSemaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
    VkSubmitInfo2 submitInfo = Renderer::VkHelpers::SubmitInfo(&commandBufferSubmitInfo, &swapchainSemaphoreWaitInfo, &renderSemaphoreSignalInfo);
    VK_CHECK(vkQueueSubmit2(vulkanContext->graphicsQueue, 1, &submitInfo, frameSync.renderFence));

    VkPresentInfoKHR presentInfo = Renderer::VkHelpers::PresentInfo(&swapchain->handle, nullptr, &swapchainImageIndex);
    presentInfo.pWaitSemaphores = &frameSync.renderSemaphore;
    const VkResult presentResult = vkQueuePresentKHR(vulkanContext->graphicsQueue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        bSwapchainOutdated = true;
        fmt::print("Swapchain out of date or suboptimal (Present)\n");
    }
}

void UIRendering::Cleanup()
{
    vkDeviceWaitIdle(vulkanContext->device);

    vkDestroyCommandPool(vulkanContext->device, immCommandPool, nullptr);
    vkDestroyFence(vulkanContext->device, immFence, nullptr);

    SDL_DestroyWindow(window);
}

void UIRendering::GenerateUIBuffer(Renderer::AllocatedBuffer& buffer, uint32_t& vertexCount)
{
    auto* vertices = static_cast<Renderer::UIVertex*>(buffer.allocationInfo.pMappedData);
    int32_t vertexIndex = 0;

    std::array<uint32_t, 2> scaledRenderExtent = renderContext->GetScaledRenderExtent();
    const float centerX = scaledRenderExtent[0] / 2.0f;
    const float centerY = scaledRenderExtent[1] / 2.0f;

    // font not implemented yet
    UIText helloText{
        .text = "Hello World!",
        .pos = {centerX + 200, centerY + 200},
        .packedColor = (0xFFFFFFFF),
        .fontIndex = 0,
        .scale = 1
    };
    UIRenderText(helloText, glyphMap, vertices, vertexIndex);

    UIRect testRect{
        .pos = {10.0f, 10.0f},
        .size = {100.0f, 100.0f},
        .packedTintColor = 0xFF00FF00,
    };
    UIRenderRect(testRect, vertices, vertexIndex);

    UIImage atlasImage{
        .pos = {scaledRenderExtent[0] - 266.0f, 10.0f},
        .size = {256.0f, 256.0f},
        .bindlessSamplerIndex = defaultSamplerIndex,
        .bindlessTextureIndex = atlasTextureIndex,
        .packedTintColor = 0xFFFFFFFF,
    };
    UIRenderImage(atlasImage, vertices, vertexIndex);

    UIRenderButton(vertices, vertexIndex, ButtonHashStr("Test Button"), {250, 250}, {200, 50}, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF, []() {
        LOG_INFO("Button Pressed");
    });
    UIText buttonText{
        .text = "Button",
        .pos = {250 + 10, 250 + 40},
        .packedColor = (0xFFFFFFFF),
        .fontIndex = 0,
        .scale = 0.75
    };
    UIRenderText(buttonText, glyphMap, vertices, vertexIndex);

    static float sliderValue = 0.5f; // Store somewhere persistent
    UIRenderSlider(vertices, vertexIndex, ButtonHashStr("volume_slider"), {100, 400}, 200.0f, sliderValue, 0.0f, 1.0f);


    vertexCount = vertexIndex;
}

void UIRendering::UIRenderRect(UIRect& rect, Renderer::UIVertex* vertices, int32_t& vertexIndex)
{
    float x0 = rect.pos.x;
    float y0 = rect.pos.y;
    float x1 = x0 + rect.size.x;
    float y1 = y0 + rect.size.y;

    vertices[vertexIndex++] = {{x0, y1}, {0.0f, 1.0f}, rect.packedTintColor, defaultSamplerIndex, whiteTextureIndex, 0};
    vertices[vertexIndex++] = {{x1, y0}, {1.0f, 0.0f}, rect.packedTintColor, defaultSamplerIndex, whiteTextureIndex, 0};
    vertices[vertexIndex++] = {{x0, y0}, {0.0f, 0.0f}, rect.packedTintColor, defaultSamplerIndex, whiteTextureIndex, 0};

    vertices[vertexIndex++] = {{x0, y1}, {0.0f, 1.0f}, rect.packedTintColor, defaultSamplerIndex, whiteTextureIndex, 0};
    vertices[vertexIndex++] = {{x1, y1}, {1.0f, 1.0f}, rect.packedTintColor, defaultSamplerIndex, whiteTextureIndex, 0};
    vertices[vertexIndex++] = {{x1, y0}, {1.0f, 0.0f}, rect.packedTintColor, defaultSamplerIndex, whiteTextureIndex, 0};
}

void UIRendering::UIRenderImage(UIImage& image, Renderer::UIVertex* vertices, int32_t& vertexIndex)
{
    float x0 = image.pos.x;
    float y0 = image.pos.y;
    float x1 = x0 + image.size.x;
    float y1 = y0 + image.size.y;

    vertices[vertexIndex++] = {{x0, y1}, {0.0f, 1.0f}, image.packedTintColor, image.bindlessSamplerIndex, image.bindlessTextureIndex, 0};
    vertices[vertexIndex++] = {{x1, y0}, {1.0f, 0.0f}, image.packedTintColor, image.bindlessSamplerIndex, image.bindlessTextureIndex, 0};
    vertices[vertexIndex++] = {{x0, y0}, {0.0f, 0.0f}, image.packedTintColor, image.bindlessSamplerIndex, image.bindlessTextureIndex, 0};

    vertices[vertexIndex++] = {{x0, y1}, {0.0f, 1.0f}, image.packedTintColor, image.bindlessSamplerIndex, image.bindlessTextureIndex, 0};
    vertices[vertexIndex++] = {{x1, y1}, {1.0f, 1.0f}, image.packedTintColor, image.bindlessSamplerIndex, image.bindlessTextureIndex, 0};
    vertices[vertexIndex++] = {{x1, y0}, {1.0f, 0.0f}, image.packedTintColor, image.bindlessSamplerIndex, image.bindlessTextureIndex, 0};
}

void UIRendering::UIRenderText(UIText& text, std::unordered_map<char, GlyphInfo>& glyphMap, Renderer::UIVertex* vertices, int32_t& vertexIndex)
{
    float cursorX = text.pos.x;
    float cursorY = text.pos.y;

    for (char c : text.text) {
        GlyphInfo& glyph = glyphMap[c];

        float x0 = cursorX + glyph.bearingX * text.scale;
        float y0 = cursorY - glyph.bearingY * text.scale;
        float x1 = x0 + glyph.width * text.scale;
        float y1 = y0 + glyph.height * text.scale;

        float u0 = static_cast<float>(glyph.atlasX) / Renderer::FONT_ATLAS_DIM;
        float v0 = static_cast<float>(glyph.atlasY) / Renderer::FONT_ATLAS_DIM;
        float u1 = static_cast<float>(glyph.atlasX + glyph.width) / Renderer::FONT_ATLAS_DIM;
        float v1 = static_cast<float>(glyph.atlasY + glyph.height) / Renderer::FONT_ATLAS_DIM;

        vertices[vertexIndex++] = {{x0, y1}, {u0, v1}, text.packedColor};
        vertices[vertexIndex++] = {{x1, y0}, {u1, v0}, text.packedColor};
        vertices[vertexIndex++] = {{x0, y0}, {u0, v0}, text.packedColor};

        vertices[vertexIndex++] = {{x0, y1}, {u0, v1}, text.packedColor};
        vertices[vertexIndex++] = {{x1, y1}, {u1, v1}, text.packedColor};
        vertices[vertexIndex++] = {{x1, y0}, {u1, v0}, text.packedColor};

        cursorX += glyph.advance * text.scale;
    }
}

void UIRendering::UIRenderButton(Renderer::UIVertex* vertices, int32_t& vertexIndex, uint32_t hash, glm::vec2 pos, glm::vec2 size, uint32_t packedBaseColor, uint32_t packedHoveredColor,
                                 uint32_t packedPressedColor, std::function<void()> onClick)
{
    UIButton& state = buttonState[hash];
    bool wasHovered = state.bIsHovered;
    bool wasPressed = state.bIsPressed;

    const Input& input = Input::Get();
    auto isMouseInRect = [&input](glm::vec2 pos, glm::vec2 size) {
        const glm::vec2 mousePos = input.GetMousePositionAbsolute();
        return mousePos.x >= pos.x &&
               mousePos.x <= pos.x + size.x &&
               mousePos.y >= pos.y &&
               mousePos.y <= pos.y + size.y;
    };


    bool isMouseDown = input.IsMouseDown(MouseButton::LMB);

    state.bIsHovered = isMouseInRect(pos, size);
    state.bIsPressed = state.bIsHovered && isMouseDown;

    // On Release
    if (wasHovered && !isMouseDown && wasPressed) {
        onClick();
    }

    uint32_t color = state.bIsPressed ? packedPressedColor : state.bIsHovered ? packedHoveredColor : packedBaseColor;

    UIRect rect{
        .pos = pos,
        .size = size,
        .packedTintColor = color,
    };
    UIRenderRect(rect, vertices, vertexIndex);
}

void UIRendering::UIRenderSlider(Renderer::UIVertex* vertices, int& vertexIndex, uint32_t hash, glm::vec2 pos, float width, float& value, float minValue, float maxValue)
{
    UISlider& state = sliderState[hash];
    const Input& input = Input::Get();
    glm::vec2 mousePos = input.GetMousePositionAbsolute();

    // Bar
    float barHeight = 4.0f;
    glm::vec2 barPos = {pos.x, pos.y - barHeight / 2};
    glm::vec2 barSize = {width, barHeight};

    // Handle
    float handleSize = 16.0f;
    float normalizedValue = (value - minValue) / (maxValue - minValue);
    float handleX = pos.x + normalizedValue * width - handleSize / 2;
    glm::vec2 handlePos = {handleX, pos.y - handleSize / 2};
    glm::vec2 handleSizeVec = {handleSize, handleSize};

    bool mouseInHandle = true;
    bool mouseInBar = false;

    if (mouseInHandle && input.IsMousePressed(MouseButton::LMB)) {
        state.bIsDragging = true;
    }

    if (!input.IsMouseDown(MouseButton::LMB)) {
        state.bIsDragging = false;
    }

    if (state.bIsDragging) {
        float t = (mousePos.x - pos.x) / width;
        value = glm::clamp(minValue + t * (maxValue - minValue), minValue, maxValue);

        LOG_INFO("Slider: {}", value);
    }

    // todo: seek if clicking bar but not handle
    UIRect bar{
        .pos = barPos,
        .size = barSize,
        .packedTintColor = 0xFF00FF00
    };
    UIRenderRect(bar, vertices, vertexIndex);

    UIRect handle{
        .pos = handlePos,
        .size = handleSizeVec,
        .packedTintColor = 0xFFFFFFFF
    };
    UIRenderRect(handle, vertices, vertexIndex);
}
}
