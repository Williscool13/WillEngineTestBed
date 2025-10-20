//
// Created by William on 2025-10-20.
//

#include "model_loading.h"

#include "VkBootstrap.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"

#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"

#include "render/vk_context.h"
#include "render/vk_swapchain.h"
#include "render/vk_imgui_wrapper.h"
#include "render/vk_descriptors.h"
#include "render/vk_pipelines.h"
#include "render/vk_helpers.h"
#include "render/render_utils.h"
#include "render/render_constants.h"
#include "render/descriptor_buffer/descriptor_buffer_combined_image_sampler.h"
#include "render/descriptor_buffer/descriptor_buffer_storage_image.h"
#include "render/descriptor_buffer/descriptor_buffer_uniform.h"

#include "input/input.h"
#include "render/model_loader.h"
#include "render/render_targets.h"
#include "utils/utils.h"

namespace Renderer
{
ModelLoading::ModelLoading() = default;

ModelLoading::~ModelLoading() = default;

void ModelLoading::Initialize()
{
    Utils::ScopedTimer timer{"Model Loading Initialization"};
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    renderContext = std::make_unique<RenderContext>(DEFAULT_SWAPCHAIN_WIDTH, DEFAULT_SWAPCHAIN_HEIGHT, DEFAULT_RENDER_SCALE);
    std::array<uint32_t, 2> renderExtent = renderContext->GetRenderExtent();

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(
        "Vulkan Test Bed",
        renderExtent[0],
        renderExtent[1],
        window_flags);

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    vulkanContext = std::make_unique<VulkanContext>(window);
    swapchain = std::make_unique<Swapchain>(vulkanContext.get(), renderExtent[0], renderExtent[1]);
    imgui = std::make_unique<ImguiWrapper>(vulkanContext.get(), window, swapchain->imageCount, swapchain->format);
    renderTargets = std::make_unique<RenderTargets>(vulkanContext.get(), DEFAULT_RENDER_TARGET_WIDTH, DEFAULT_RENDER_TARGET_HEIGHT);
    Input::Input::Get().Init(window, swapchain->extent.width, swapchain->extent.height);

    renderFramesInFlight = swapchain->imageCount;

    frameSynchronization.reserve(renderFramesInFlight);
    for (int32_t i = 0; i < renderFramesInFlight; ++i) {
        frameSynchronization.emplace_back(vulkanContext.get());
        frameSynchronization[i].Initialize();
    }

    ModelLoader mLoader{vulkanContext.get()};
    auto model = mLoader.LoadGltf(std::filesystem::path("../assets/BoxTextured.glb"));
    LOG_INFO("Model Sampler Count: {}", model.samplers.size());
    LOG_INFO("Model Image Count: {}", model.images.size());
    LOG_INFO("Model Image View Count: {}", model.imageViews.size());
}

void ModelLoading::Run()
{}

void ModelLoading::Render()
{}

void ModelLoading::Cleanup()
{}
}
