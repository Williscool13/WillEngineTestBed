//
// Created by William on 2025-10-20.
//

#include "model_loading.h"

#include <VkBootstrap.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/glm.hpp>

#include "offsetAllocator.hpp"
#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"

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
#include "../src/render/model/model_loader.h"
#include "core/time.h"
#include "render/render_targets.h"
#include "utils/utils.h"
#include "utils/world_constants.h"

namespace Renderer
{
ModelLoading::ModelLoading() = default;

ModelLoading::~ModelLoading() = default;

void ModelLoading::CreateResources()
{
    DescriptorLayoutBuilder layoutBuilder{2};
    //
    {
        layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1); // Draw Image
        layoutBuilder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1); // Depth Image
        // Add render targets as needed
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = layoutBuilder.Build(
            static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT),
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
        );
        renderTargetSetLayout = VkResources::CreateDescriptorSetLayout(vulkanContext.get(), layoutCreateInfo);
        renderTargetDescriptors = DescriptorBufferStorageImage(vulkanContext.get(), renderTargetSetLayout.handle, 1);
        renderTargetDescriptors.AllocateDescriptorSet();
    }

    VkDescriptorImageInfo drawDescriptorInfo;
    drawDescriptorInfo.imageView = renderTargets->drawImageView.handle;
    drawDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    renderTargetDescriptors.UpdateDescriptor(drawDescriptorInfo, 0, 0, 0);

    bindlessResourcesDescriptorBuffer = DescriptorBufferBindlessResources(vulkanContext.get());

    //
    {
        VkPipelineLayoutCreateInfo computePipelineLayoutCreateInfo{};
        computePipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        computePipelineLayoutCreateInfo.pNext = nullptr;
        computePipelineLayoutCreateInfo.pSetLayouts = nullptr;
        computePipelineLayoutCreateInfo.setLayoutCount = 0;

        VkPushConstantRange pushConstant{};
        pushConstant.offset = 0;
        pushConstant.size = sizeof(BindlessIndirectPushConstant);
        pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        computePipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
        computePipelineLayoutCreateInfo.pushConstantRangeCount = 1;

        drawCullPipelineLayout = VkResources::CreatePipelineLayout(vulkanContext.get(), computePipelineLayoutCreateInfo);

        VkShaderModule computeShader;
        if (!VkHelpers::LoadShaderModule("shaders\\drawCull_compute.spv", vulkanContext->device, &computeShader)) {
            throw std::runtime_error("Error when building the compute shader (drawCull_compute.spv)");
        }

        VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = VkHelpers::PipelineShaderStageCreateInfo(computeShader, VK_SHADER_STAGE_COMPUTE_BIT);
        VkComputePipelineCreateInfo computePipelineCreateInfo = VkHelpers::ComputePipelineCreateInfo(drawCullPipelineLayout.handle, pipelineShaderStageCreateInfo);
        drawCullPipeline = VkResources::CreateComputePipeline(vulkanContext.get(), computePipelineCreateInfo);

        // Cleanup
        vkDestroyShaderModule(vulkanContext->device, computeShader, nullptr);
    }


    //
    {
        VkPushConstantRange renderPushConstantRange{};
        renderPushConstantRange.offset = 0;
        renderPushConstantRange.size = sizeof(BindlessAddressPushConstant);
        renderPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkPipelineLayoutCreateInfo renderPipelineLayoutCreateInfo{};
        renderPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        renderPipelineLayoutCreateInfo.pSetLayouts = &bindlessResourcesDescriptorBuffer.descriptorSetLayout.handle;
        renderPipelineLayoutCreateInfo.setLayoutCount = 1;
        //renderPipelineLayoutCreateInfo.pSetLayouts = nullptr;
        //renderPipelineLayoutCreateInfo.setLayoutCount = 0;
        renderPipelineLayoutCreateInfo.pPushConstantRanges = &renderPushConstantRange;
        renderPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

        renderPipelineLayout = VkResources::CreatePipelineLayout(vulkanContext.get(), renderPipelineLayoutCreateInfo);

        VkShaderModule vertShader;
        VkShaderModule fragShader;
        if (!VkHelpers::LoadShaderModule("shaders\\indirectDraw_vertex.spv", vulkanContext->device, &vertShader)) {
            throw std::runtime_error("Error when building the vertex shader (indirectDraw_vertex.spv)");
        }
        if (!VkHelpers::LoadShaderModule("shaders\\indirectDraw_fragment.spv", vulkanContext->device, &fragShader)) {
            throw std::runtime_error("Error when building the fragment shader (indirectDraw_fragment.spv)");
        }


        RenderPipelineBuilder renderPipelineBuilder;

        const std::vector<VkVertexInputBindingDescription> vertexBindings{
            {
                .binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
            {
                .binding = 1,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            }
        };

        const std::vector<VkVertexInputAttributeDescription> vertexAttributes{
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, position),
            },
            {
                .location = 1,
                .binding = 1,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, normal),
            },
            {
                .location = 2,
                .binding = 1,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(Vertex, tangent),
            },
            {
                .location = 3,
                .binding = 1,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(Vertex, color),
            },
            {
                .location = 4,
                .binding = 1,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(Vertex, uv),
            },
            // todo: different vertex input attribute for normal vs skeletal pipelines
            {
                .location = 5,
                .binding = 1,
                .format = VK_FORMAT_R32G32B32A32_UINT,
                .offset = offsetof(Vertex, joints),
            },
            {
                .location = 6,
                .binding = 1,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(Vertex, weights),
            }
        };

        renderPipelineBuilder.setupVertexInput(vertexBindings, vertexAttributes);

        renderPipelineBuilder.setShaders(vertShader, fragShader);
        renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        renderPipelineBuilder.disableMultisampling();
        renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
        renderPipelineBuilder.setupRenderer({DRAW_IMAGE_FORMAT}, DEPTH_IMAGE_FORMAT);
        renderPipelineBuilder.setupPipelineLayout(renderPipelineLayout.handle);
        VkGraphicsPipelineCreateInfo pipelineCreateInfo = renderPipelineBuilder.generatePipelineCreateInfo();
        renderPipeline = VkResources::CreateGraphicsPipeline(vulkanContext.get(), pipelineCreateInfo);

        vkDestroyShaderModule(vulkanContext->device, vertShader, nullptr);
        vkDestroyShaderModule(vulkanContext->device, fragShader, nullptr);


        VkPushConstantRange skeletalPushConstantRange{};
        skeletalPushConstantRange.offset = 0;
        skeletalPushConstantRange.size = sizeof(BindlessAddressSkeletalPushConstant);
        skeletalPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkPipelineLayoutCreateInfo skeletalPipelineLayoutCreateInfo{};
        skeletalPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        skeletalPipelineLayoutCreateInfo.pSetLayouts = &bindlessResourcesDescriptorBuffer.descriptorSetLayout.handle;
        skeletalPipelineLayoutCreateInfo.setLayoutCount = 1;
        //renderPipelineLayoutCreateInfo.pSetLayouts = nullptr;
        //renderPipelineLayoutCreateInfo.setLayoutCount = 0;
        skeletalPipelineLayoutCreateInfo.pPushConstantRanges = &skeletalPushConstantRange;
        skeletalPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

        skeletalPipelineLayout = VkResources::CreatePipelineLayout(vulkanContext.get(), skeletalPipelineLayoutCreateInfo);

        if (!VkHelpers::LoadShaderModule("shaders\\skeletalIndirectDraw_vertex.spv", vulkanContext->device, &vertShader)) {
            throw std::runtime_error("Error when building the vertex shader (skeletalIndirectDraw_vertex.spv)");
        }
        if (!VkHelpers::LoadShaderModule("shaders\\skeletalIndirectDraw_fragment.spv", vulkanContext->device, &fragShader)) {
            throw std::runtime_error("Error when building the fragment shader (skeletalIndirectDraw_fragment.spv)");
        }

        renderPipelineBuilder.setShaders(vertShader, fragShader);
        renderPipelineBuilder.setupPipelineLayout(skeletalPipelineLayout.handle);
        pipelineCreateInfo = renderPipelineBuilder.generatePipelineCreateInfo();
        skeletalPipeline = VkResources::CreateGraphicsPipeline(vulkanContext.get(), pipelineCreateInfo);


        vkDestroyShaderModule(vulkanContext->device, vertShader, nullptr);
        vkDestroyShaderModule(vulkanContext->device, fragShader, nullptr);
    }
}

void ModelLoading::CreateModels()
{
    // todo: optimize so that it's GPU only, use a staging buffer to upload in production
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    bufferInfo.usage = VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT;
    bufferInfo.size = sizeof(Vertex) * MEGA_VERTEX_BUFFER_COUNT;
    megaVertexBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT;
    bufferInfo.size = sizeof(uint32_t) * MEGA_INDEX_BUFFER_COUNT;
    megaIndexBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(MaterialProperties) * MEGA_MATERIAL_BUFFER_COUNT;
    materialBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Primitive) * MEGA_PRIMITIVE_BUFFER_COUNT;
    primitiveBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);

    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT;
    bufferInfo.size = sizeof(VkDrawIndexedIndirectCommand) * BINDLESS_INSTANCE_COUNT;
    opaqueIndexedIndirectBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);

    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT;
    bufferInfo.size = sizeof(VkDrawIndexedIndirectCommand) * BINDLESS_INSTANCE_COUNT;
    opaqueSkeletalIndexedIndirectBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);

    indirectCountBuffers.reserve(swapchain->imageCount);
    skeletalIndirectCountBuffers.reserve(swapchain->imageCount);
    for (int32_t i = 0; i < swapchain->imageCount; ++i) {
        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.size = sizeof(IndirectCount);
        indirectCountBuffers.push_back(VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.size = sizeof(IndirectCount);
        skeletalIndirectCountBuffers.push_back(VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.size = sizeof(Model) * BINDLESS_MODEL_MATRIX_COUNT;
        modelBuffers.push_back(VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.size = sizeof(Instance) * BINDLESS_INSTANCE_COUNT;
        instanceBuffers.push_back(VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.size = sizeof(Model) * BINDLESS_MODEL_MATRIX_COUNT;
        jointMatrixBuffers.push_back(VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));
    }


    // Suzanne Model
    ModelData* md;

    auto suzannePath = std::filesystem::path("../assets/Suzanne/glTF/Suzanne.gltf");
    ModelDataHandle suzanneHandle = modelDatas.Add();
    modelDataHandles.push_back(suzanneHandle);
    md = modelDatas.Get(suzanneHandle);
    LoadModelIntoBuffers(suzannePath, *md);

    auto boxPath = std::filesystem::path("../assets/BoxTextured.glb");
    ModelDataHandle boxHandle = modelDatas.Add();
    modelDataHandles.push_back(boxHandle);
    md = modelDatas.Get(boxHandle);
    LoadModelIntoBuffers(boxPath, *md);

    auto riggedFigurePath = std::filesystem::path("../assets/RiggedFigure.glb");
    ModelDataHandle figureHandle = modelDatas.Add();
    modelDataHandles.push_back(figureHandle);
    md = modelDatas.Get(figureHandle);
    LoadModelIntoBuffers(riggedFigurePath, *md);

    auto structurePath = std::filesystem::path("../assets/structure.glb");
    structureHandle = modelDatas.Add();
    modelDataHandles.push_back(structureHandle);
    md = modelDatas.Get(structureHandle);
    LoadModelIntoBuffers(structurePath, *md);

    auto simpleSkin = std::filesystem::path("../assets/simpleSkin/SimpleSkin.gltf");
    simpleSkinHandle = modelDatas.Add();
    modelDataHandles.push_back(simpleSkinHandle);
    md = modelDatas.Get(simpleSkinHandle);
    LoadModelIntoBuffers(simpleSkin, *md);

    runtimeMeshes.reserve(100);


    // for (int y = 0; y < 3; ++y) {
    //     for (int x = 0; x < 3; ++x) {
    //         Transform boxTransform{
    //             glm::vec3(x * 2.0f, y * 2.0f, 0.0f),
    //             glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
    //             glm::vec3(1.0f, 1.0f, 1.0f)
    //         };
    //
    //         RuntimeMesh a = GenerateModel(suzanneHandle, boxTransform);
    //         UpdateTransforms(a);
    //         runtimeMeshes.push_back(std::move(a));
    //     }
    // }

    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            Transform boxTransform{
                glm::vec3(x * 2.0f, y * 2.0f, 5.0f),
                glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 5.0f)),
                glm::vec3(1.0f, 1.0f, 1.0f)
            };

            RuntimeMesh a = GenerateModel(boxHandle, boxTransform);
            UpdateTransforms(a);
            runtimeMeshes.push_back(std::move(a));
        }
    }

    // RuntimeMesh a = GenerateModel(structureHandle, Transform::Identity);
    // UpdateTransforms(a);
    // runtimeMeshes.push_back(std::move(a));

    RuntimeMesh a = GenerateModel(simpleSkinHandle, Transform::Identity);
    UpdateTransforms(a);
    runtimeMeshes.push_back(std::move(a));
    simpleRiggedRuntimeMesh = &runtimeMeshes.back();

    for (RuntimeMesh& runtimeMesh : runtimeMeshes) {
        InitialUploadRuntimeMesh(runtimeMesh);
    }
}

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

    window = SDL_CreateWindow("Vulkan Test Bed", renderExtent[0], renderExtent[1], window_flags);

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

    modelLoader = std::make_unique<ModelLoader>(vulkanContext.get());

    for (int32_t i = 0; i < swapchain->imageCount; ++i) {
        VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.pNext = nullptr;
        VmaAllocationCreateInfo vmaAllocInfo = {};
        vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
        bufferInfo.size = sizeof(SceneData);
        sceneDataBuffers.push_back(VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo));
    }


    CreateResources();

    CreateModels();
}

void ModelLoading::Run()
{
    Input& input = Input::Input::Get();
    Core::Time& time = Time::Get();

    animationPlayer.Play(modelDatas.Get(simpleSkinHandle)->animations[0], true);

    SDL_Event e;
    bool exit = false;
    while (true) {
        while (SDL_PollEvent(&e) != 0) {
            input.ProcessEvent(e);
            imgui->HandleInput(e);
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
            if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                bSwapchainOutdated = true;
            }
        }
        input.UpdateFocus(SDL_GetWindowFlags(window));
        time.Update();

        if (bSwapchainOutdated) {
            vkDeviceWaitIdle(vulkanContext->device);

            int32_t w, h;
            SDL_GetWindowSize(window, &w, &h);

            swapchain->Recreate(w, h);
            Input::Input::Get().UpdateWindowExtent(swapchain->extent.width, swapchain->extent.height);
            if (RENDER_TARGET_SIZE_EQUALS_SWAPCHAIN_SIZE) {
                renderContext->RequestRenderExtentResize(w, h);
            }
            bSwapchainOutdated = false;
        }

        if (renderContext->HasPendingRenderExtentChanges()) {
            vkDeviceWaitIdle(vulkanContext->device);
            renderContext->ApplyRenderExtentResize();

            std::array<uint32_t, 2> newExtents = renderContext->GetRenderExtent();
            renderTargets->Recreate(newExtents[0], newExtents[1]);

            // Upload to descriptor buffer
            VkDescriptorImageInfo drawDescriptorInfo;
            drawDescriptorInfo.imageView = renderTargets->drawImageView.handle;
            drawDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            renderTargetDescriptors.UpdateDescriptor(drawDescriptorInfo, 0, 0, 0);
        }


        if (exit) {
            bShouldExit = true;
            break;
        }

        Render();

        input.FrameReset();
        frameNumber++;
    }
}

void ModelLoading::Render()
{
    std::array<uint32_t, 2> scaledRenderExtent = renderContext->GetScaledRenderExtent();
    Input& input = Input::Input::Get();
    const float deltaTime = Time::Get().GetDeltaTime();

    const uint32_t currentFrameInFlight = frameNumber % swapchain->imageCount;
    const FrameData& currentFrameData = frameSynchronization[currentFrameInFlight];

    // Wait for the GPU to finish the last frame that used this frame-in-flight's resources (N - imageCount).
    VK_CHECK(vkWaitForFences(vulkanContext->device, 1, &currentFrameData.renderFence, true, 1000000000));
    // Un-signal fence, essentially saying "I'm using this frame-in-flight's resources, hands off" (when the command is submitted).
    VK_CHECK(vkResetFences(vulkanContext->device, 1, &currentFrameData.renderFence));

    uint32_t swapchainImageIndex;
    // (Non-Blocking) Acquire swapchain image index. Signal semaphore when the actual image is ready for use.
    VkResult e = vkAcquireNextImageKHR(vulkanContext->device, swapchain->handle, 1000000000, currentFrameData.swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        bSwapchainOutdated = true;
        LOG_WARN("Swapchain out of date or suboptimal (Acquire)");
        return;
    }

    VkImage currentSwapchainImage = swapchain->swapchainImages[swapchainImageIndex];
    VkImageView currentSwapchainImageView = swapchain->swapchainImageViews[swapchainImageIndex];


    // Do rendering stuff
    VkCommandBuffer cmd = currentFrameData.commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VkCommandBufferBeginInfo commandBufferBeginInfo = VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBeginInfo));

    //
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("Main")) {
            ImGui::Text("Hello!");
            ImGui::DragFloat3("Position", cameraPos);
            ImGui::DragFloat3("Look", cameraLook);
            ImGui::DragFloat3("Box Pos", boxPos);
        }


        ImGui::End();
        ImGui::Render();
    }

    glm::mat4 view = glm::lookAt(
        glm::vec3(cameraPos[0], cameraPos[1], cameraPos[2]),
        glm::vec3(cameraLook[0], cameraLook[1], cameraLook[2]),
        WORLD_UP);

    glm::mat4 proj = glm::perspective(
        glm::radians(75.0f),
        static_cast<float>(scaledRenderExtent[0]) / static_cast<float>(scaledRenderExtent[1]),
        1000.0f,
        0.1f);

    sceneData.prevView = sceneData.view;
    sceneData.prevProj = sceneData.proj;
    sceneData.prevViewProj = sceneData.viewProj;
    sceneData.view = view;
    sceneData.proj = proj;
    sceneData.viewProj = proj * view;
    sceneData.renderTargetSize.x = scaledRenderExtent[0];
    sceneData.renderTargetSize.y = scaledRenderExtent[1];

    sceneData.deltaTime = deltaTime;

    AllocatedBuffer& currentSceneDataBuffer = sceneDataBuffers[currentFrameInFlight];
    SceneData* currentSceneData = static_cast<SceneData*>(currentSceneDataBuffer.allocationInfo.pMappedData);
    *currentSceneData = sceneData;


    // RuntimeMesh& runtimeMesh = runtimeMeshes[18];
    // // Update on CPU
    // {
    //     const float deltaTime = Time::Get().GetDeltaTime();
    //     static float time = 0.0f;
    //     time += deltaTime;
    //     float yOffset = 5.0f * sinf(time * 0.5f);
    //     runtimeMesh.transform.translation.y = yOffset;
    //     UpdateTransforms(runtimeMesh);
    // }

    animationPlayer.Update(deltaTime, simpleRiggedRuntimeMesh->nodes, simpleRiggedRuntimeMesh->nodeRemap);
    UpdateTransforms(*simpleRiggedRuntimeMesh);
    UpdateRuntimeMesh(*simpleRiggedRuntimeMesh, modelBuffers[currentFrameInFlight], jointMatrixBuffers[currentFrameInFlight]);
    //
    // // GPU only needs to know all:
    // //  - model matrix index
    // //  - mat4 model matrix
    // //  - model matrix used for this FIF
    // UpdateRuntimeMesh(runtimeMesh, modelBuffers[currentFrameInFlight]);


    //
    {
        // Draw/Cull pass (compute) - Construct indexed indirect buffer
        {
            {
                vkCmdFillBuffer(cmd, indirectCountBuffers[currentFrameInFlight].handle,offsetof(IndirectCount, opaqueCount), sizeof(uint32_t), 0);
                vkCmdFillBuffer(cmd, skeletalIndirectCountBuffers[currentFrameInFlight].handle,offsetof(IndirectCount, opaqueCount), sizeof(uint32_t), 0);
                VkBufferMemoryBarrier2 bufferBarrier[2];
                bufferBarrier[0] = VkHelpers::BufferMemoryBarrier(
                    indirectCountBuffers[currentFrameInFlight].handle, 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
                bufferBarrier[1] = VkHelpers::BufferMemoryBarrier(
                    skeletalIndirectCountBuffers[currentFrameInFlight].handle, 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

                VkDependencyInfo depInfo{};
                depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                depInfo.pNext = nullptr;
                depInfo.dependencyFlags = 0;
                depInfo.bufferMemoryBarrierCount = 2;
                depInfo.pBufferMemoryBarriers = bufferBarrier;

                vkCmdPipelineBarrier2(cmd, &depInfo);

                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, drawCullPipeline.handle);
                BindlessIndirectPushConstant pushData{
                    currentSceneDataBuffer.address,
                    primitiveBuffer.address,
                    modelBuffers[currentFrameInFlight].address,
                    instanceBuffers[currentFrameInFlight].address,
                    opaqueIndexedIndirectBuffer.address,
                    indirectCountBuffers[currentFrameInFlight].address,
                    opaqueSkeletalIndexedIndirectBuffer.address,
                    skeletalIndirectCountBuffers[currentFrameInFlight].address,
                };

                vkCmdPushConstants(cmd, drawCullPipelineLayout.handle, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BindlessIndirectPushConstant), &pushData);
                uint32_t groupsX = (highestInstanceIndex + 63) / 64;
                vkCmdDispatch(cmd, groupsX, 1, 1);
            }

            //
            {
                VkBufferMemoryBarrier2 bufferBarrier[4];
                bufferBarrier[0] = VkHelpers::BufferMemoryBarrier(
                    opaqueIndexedIndirectBuffer.handle, 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
                bufferBarrier[1] = VkHelpers::BufferMemoryBarrier(
                    indirectCountBuffers[currentFrameInFlight].handle, 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
                bufferBarrier[2] = VkHelpers::BufferMemoryBarrier(
                    opaqueSkeletalIndexedIndirectBuffer.handle, 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
                bufferBarrier[3] = VkHelpers::BufferMemoryBarrier(
                    skeletalIndirectCountBuffers[currentFrameInFlight].handle, 0, VK_WHOLE_SIZE,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);

                VkDependencyInfo depInfo{};
                depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                depInfo.pNext = nullptr;
                depInfo.dependencyFlags = 0;
                depInfo.bufferMemoryBarrierCount = 4;
                depInfo.pBufferMemoryBarriers = bufferBarrier;

                vkCmdPipelineBarrier2(cmd, &depInfo);
            }
        }

        // Transition 1
        {
            auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
            auto barrier = VkHelpers::ImageMemoryBarrier(
                renderTargets->drawImage.handle,
                subresource,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );
            auto dependencyInfo = VkHelpers::DependencyInfo(&barrier);
            vkCmdPipelineBarrier2(cmd, &dependencyInfo);
        }

        // Draw render
        {
            constexpr VkClearValue colorClear = {.color = {0.0f, 0.0f, 0.0f, 1.0f}};
            const VkRenderingAttachmentInfo colorAttachment = VkHelpers::RenderingAttachmentInfo(renderTargets->drawImageView.handle, &colorClear, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            constexpr VkClearValue depthClear = {.depthStencil = {0.0f, 0u}};
            const VkRenderingAttachmentInfo depthAttachment = VkHelpers::RenderingAttachmentInfo(renderTargets->depthImageView.handle, &depthClear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
            const VkRenderingInfo renderInfo = VkHelpers::RenderingInfo({scaledRenderExtent[0], scaledRenderExtent[1]}, &colorAttachment, &depthAttachment);


            vkCmdBeginRendering(cmd, &renderInfo);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPipeline.handle);

            VkViewport viewport = VkHelpers::GenerateViewport(scaledRenderExtent[0], scaledRenderExtent[1]);
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            VkRect2D scissor = VkHelpers::GenerateScissor(scaledRenderExtent[0], scaledRenderExtent[1]);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            BindlessAddressPushConstant pushData{
                currentSceneDataBuffer.address,
                materialBuffer.address,
                primitiveBuffer.address,
                modelBuffers[currentFrameInFlight].address,
                instanceBuffers[currentFrameInFlight].address,
            };

            vkCmdPushConstants(cmd, renderPipelineLayout.handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BindlessAddressPushConstant), &pushData);

            VkDescriptorBufferBindingInfoEXT bindingInfo = bindlessResourcesDescriptorBuffer.GetBindingInfo();
            vkCmdBindDescriptorBuffersEXT(cmd, 1, &bindingInfo);

            uint32_t bufferIndexImage = 0;
            VkDeviceSize bufferOffset = 0;
            vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPipelineLayout.handle, 0, 1, &bufferIndexImage, &bufferOffset);


            const VkBuffer vertexBuffers[2] = {megaVertexBuffer.handle, megaVertexBuffer.handle};
            constexpr VkDeviceSize vertexOffsets[2] = {0, 0};
            vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, vertexOffsets);
            vkCmdBindIndexBuffer(cmd, megaIndexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexedIndirectCount(cmd,
                                          opaqueIndexedIndirectBuffer.handle, 0,
                                          indirectCountBuffers[currentFrameInFlight].handle, offsetof(IndirectCount, opaqueCount),
                                          highestInstanceIndex, sizeof(VkDrawIndexedIndirectCommand));

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skeletalPipeline.handle);
            BindlessAddressSkeletalPushConstant skeletalPushData{
                currentSceneDataBuffer.address,
                materialBuffer.address,
                primitiveBuffer.address,
                modelBuffers[currentFrameInFlight].address,
                instanceBuffers[currentFrameInFlight].address,
                jointMatrixBuffers[currentFrameInFlight].address,
            };
            vkCmdPushConstants(cmd, skeletalPipelineLayout.handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BindlessAddressSkeletalPushConstant), &skeletalPushData);

            vkCmdDrawIndexedIndirectCount(cmd,
                                          opaqueSkeletalIndexedIndirectBuffer.handle, 0,
                                          skeletalIndirectCountBuffers[currentFrameInFlight].handle, offsetof(IndirectCount, opaqueCount),
                                          highestInstanceIndex, sizeof(VkDrawIndexedIndirectCommand));

            vkCmdEndRendering(cmd);
        }

        // Transition 2 - Prepare for copy
        {
            VkImageMemoryBarrier2 barriers[2];
            barriers[0] = VkHelpers::ImageMemoryBarrier(
                renderTargets->drawImage.handle,
                VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            );
            barriers[1] = VkHelpers::ImageMemoryBarrier(
                currentSwapchainImage,
                VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );
            VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            depInfo.imageMemoryBarrierCount = 2;
            depInfo.pImageMemoryBarriers = barriers;
            vkCmdPipelineBarrier2(cmd, &depInfo);
        }

        // Copy
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

        // Transition 3 - Prepare for imgui draw
        {
            auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
            auto barrier = VkHelpers::ImageMemoryBarrier(
                currentSwapchainImage,
                subresource,
                VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            );
            auto dependencyInfo = VkHelpers::DependencyInfo(&barrier);
            vkCmdPipelineBarrier2(cmd, &dependencyInfo);
        }
    }

    // Imgui Draw
    {
        VkRenderingAttachmentInfo imguiAttachment{};
        imguiAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        imguiAttachment.pNext = nullptr;
        imguiAttachment.imageView = currentSwapchainImageView;
        imguiAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imguiAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        imguiAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkRenderingInfo renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.pNext = nullptr;
        renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, swapchain->extent};
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &imguiAttachment;
        renderInfo.pDepthAttachment = nullptr;
        renderInfo.pStencilAttachment = nullptr;

        vkCmdBeginRendering(cmd, &renderInfo);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        vkCmdEndRendering(cmd);
    }


    // Final transition -
    {
        auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        auto barrier = VkHelpers::ImageMemoryBarrier(
            currentSwapchainImage,
            subresource,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );
        auto dependencyInfo = VkHelpers::DependencyInfo(&barrier);
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }


    VK_CHECK(vkEndCommandBuffer(cmd));


    VkCommandBufferSubmitInfo commandBufferSubmitInfo = VkHelpers::CommandBufferSubmitInfo(currentFrameData.commandBuffer);
    VkSemaphoreSubmitInfo swapchainSemaphoreWaitInfo = VkHelpers::SemaphoreSubmitInfo(currentFrameData.swapchainSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR);
    VkSemaphoreSubmitInfo renderSemaphoreSignalInfo = VkHelpers::SemaphoreSubmitInfo(currentFrameData.renderSemaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
    VkSubmitInfo2 submitInfo = VkHelpers::SubmitInfo(&commandBufferSubmitInfo, &swapchainSemaphoreWaitInfo, &renderSemaphoreSignalInfo);

    // Wait for swapchain semaphore, then submit command buffer. When finished, signal render semaphore and render fence.
    VK_CHECK(vkQueueSubmit2(vulkanContext->graphicsQueue, 1, &submitInfo, currentFrameData.renderFence));

    // Wait for render semaphore, then present frame.
    VkPresentInfoKHR presentInfo = VkHelpers::PresentInfo(&swapchain->handle, nullptr, &swapchainImageIndex);
    presentInfo.pWaitSemaphores = &currentFrameData.renderSemaphore;
    const VkResult presentResult = vkQueuePresentKHR(vulkanContext->graphicsQueue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        bSwapchainOutdated = true;
        fmt::print("Swapchain out of date or suboptimal (Present)\n");
    }
}

void ModelLoading::Cleanup()
{
    vkDeviceWaitIdle(vulkanContext->device);

    SDL_DestroyWindow(window);
}

bool ModelLoading::LoadModelIntoBuffers(const std::filesystem::path& modelPath, ModelData& modelData)
{
    ExtractedModel model = modelLoader->LoadGltf(modelPath);
    if (!model.bSuccessfullyLoaded) {
        return false;
    }

    modelData.name = modelPath.filename().string();
    modelData.path = modelPath;

    // Vertices
    size_t sizeVertexPos = model.vertices.size() * sizeof(Vertex);
    modelData.vertexPositionAllocation = vertexBufferAllocator.allocate(sizeVertexPos);
    if (modelData.vertexPositionAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in vertex buffer");
        return false;
    }
    memcpy(static_cast<char*>(megaVertexBuffer.allocationInfo.pMappedData) + modelData.vertexPositionAllocation.offset, model.vertices.data(), sizeVertexPos);

    // Indices
    size_t sizeIndices = model.indices.size() * sizeof(uint32_t);
    modelData.indexAllocation = indexBufferAllocator.allocate(sizeIndices);
    if (modelData.indexAllocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
        LOG_WARN("[ModelLoading::LoadModelIntoBuffers] Not enough space in index buffer");
        return false;
    }
    memcpy(static_cast<char*>(megaIndexBuffer.allocationInfo.pMappedData) + modelData.indexAllocation.offset, model.indices.data(), sizeIndices);

    auto RemapIndices = [](auto& indices, const auto& map) {
        indices.x = map.at(indices.x);
        indices.y = map.at(indices.y);
        indices.z = map.at(indices.z);
        indices.w = map.at(indices.w);
    };

    std::unordered_map<int32_t, int32_t> materialToBufferMap;
    materialToBufferMap[-1] = -1;

    // Samplers
    for (int32_t i = 0; i < model.samplers.size(); ++i) {
        materialToBufferMap[i] = bindlessResourcesDescriptorBuffer.AllocateSampler(model.samplers[i].handle);
    }

    for (MaterialProperties& material : model.materials) {
        RemapIndices(material.textureSamplerIndices, materialToBufferMap);
        RemapIndices(material.textureSamplerIndices2, materialToBufferMap);
    }

    modelData.samplerIndexToDescriptorBufferIndexMap = std::move(materialToBufferMap);

    // Textures
    materialToBufferMap.clear();
    materialToBufferMap[-1] = -1;

    for (int32_t i = 0; i < model.imageViews.size(); ++i) {
        materialToBufferMap[i] = bindlessResourcesDescriptorBuffer.AllocateTexture({
            .imageView = model.imageViews[i].handle,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });
    }

    for (MaterialProperties& material : model.materials) {
        RemapIndices(material.textureImageIndices, materialToBufferMap);
        RemapIndices(material.textureImageIndices2, materialToBufferMap);
    }

    modelData.textureIndexToDescriptorBufferIndexMap = std::move(materialToBufferMap);

    // Materials
    size_t sizeMaterials = model.materials.size() * sizeof(MaterialProperties);
    modelData.materialAllocation = materialBufferAllocator.allocate(sizeMaterials);
    memcpy(static_cast<char*>(materialBuffer.allocationInfo.pMappedData) + modelData.materialAllocation.offset, model.materials.data(), sizeMaterials);

    // Primitives
    uint32_t firstIndexCount = modelData.indexAllocation.offset / sizeof(uint32_t);
    uint32_t vertexOffsetCount = modelData.vertexPositionAllocation.offset / sizeof(Vertex);
    uint32_t materialOffsetCount = modelData.materialAllocation.offset / sizeof(MaterialProperties);

    for (auto& primitive : model.primitives) {
        primitive.firstIndex += firstIndexCount;
        primitive.vertexOffset += static_cast<int32_t>(vertexOffsetCount);
        primitive.materialIndex += materialOffsetCount;
    }

    size_t sizePrimitives = model.primitives.size() * sizeof(Primitive);
    modelData.primitiveAllocation = primitiveBufferAllocator.allocate(sizePrimitives);
    memcpy(static_cast<char*>(primitiveBuffer.allocationInfo.pMappedData) + modelData.primitiveAllocation.offset,
           model.primitives.data(), sizePrimitives);

    uint32_t primitiveOffsetCount = modelData.primitiveAllocation.offset / sizeof(Primitive);
    modelData.meshes = std::move(model.meshes);
    for (auto& mesh : modelData.meshes) {
        for (auto& primitiveIndex : mesh.primitiveIndices) {
            primitiveIndex += primitiveOffsetCount;
        }
    }

    modelData.samplers = std::move(model.samplers);
    modelData.images = std::move(model.images);
    modelData.nodes = std::move(model.nodes);

    modelData.inverseBindMatrices = std::move(model.inverseBindMatrices);
    modelData.animations = std::move(model.animations);
    modelData.nodeRemap = std::move(model.nodeRemap);

    return true;
}

RuntimeMesh ModelLoading::GenerateModel(ModelDataHandle modelDataHandle, const Transform& topLevelTransform)
{
    RuntimeMesh rm{};

    const ModelData* modelData = modelDatas.Get(modelDataHandle);
    if (!modelData) { return rm; }

    rm.transform = topLevelTransform;
    rm.nodes.reserve(modelData->nodes.size());
    rm.nodeRemap = modelData->nodeRemap;

    size_t jointMatrixCount = modelData->inverseBindMatrices.size();
    bool bHasSkinning = jointMatrixCount > 0;
    if (bHasSkinning) {
        rm.jointMatrixAllocation = jointMatrixAllocator.allocate(jointMatrixCount * sizeof(Model));
        rm.jointMatrixOffset = rm.jointMatrixAllocation.offset / sizeof(uint32_t);
    }

    rm.modelDataHandle = modelDataHandle;
    for (const Node& n : modelData->nodes) {
        rm.nodes.emplace_back(n);
        RuntimeNode& rn = rm.nodes.back();
        if (n.inverseBindIndex != ~0u) {
            rn.inverseBindMatrix = modelData->inverseBindMatrices[n.inverseBindIndex];
        }
    }


    return rm;
}

void ModelLoading::UpdateTransforms(RuntimeMesh& runtimeMesh)
{
    glm::mat4 baseTopLevel = runtimeMesh.transform.GetMatrix();
    for (RuntimeNode& rn : runtimeMesh.nodes) {
        glm::mat4 worldTransform = rn.transform.GetMatrix();
        uint32_t currentParent = rn.parent;
        while (currentParent != ~0u) {
            worldTransform = runtimeMesh.nodes[currentParent].transform.GetMatrix() * worldTransform;
            currentParent = runtimeMesh.nodes[currentParent].parent;
        }

        rn.cachedWorldTransform = baseTopLevel * worldTransform;
    }
}

void ModelLoading::InitialUploadRuntimeMesh(RuntimeMesh& runtimeMesh)
{
    for (RuntimeNode& node : runtimeMesh.nodes) {
        if (node.meshIndex != ~0u) {
            node.modelMatrixHandle = modelMatrixAllocator.Add();

            for (int32_t i = 0; i < swapchain->imageCount; ++i) {
                memcpy(
                    static_cast<char*>(modelBuffers[i].allocationInfo.pMappedData) + node.modelMatrixHandle.index * sizeof(Model) + offsetof(Model, modelMatrix),
                    &node.cachedWorldTransform,
                       sizeof(node.cachedWorldTransform));
            }

            if (ModelData* modelData = modelDatas.Get(runtimeMesh.modelDataHandle)) {
                for (uint32_t primitiveIndex : modelData->meshes[node.meshIndex].primitiveIndices) {
                    InstanceEntryHandle instanceEntry = instanceEntryAllocator.Add();
                    node.instanceEntryHandles.push_back(instanceEntry);

                    if ((instanceEntry.index + 1) > highestInstanceIndex) {
                        highestInstanceIndex = instanceEntry.index + 1;
                    }

                    Instance inst;
                    inst.modelIndex = node.modelMatrixHandle.index;
                    inst.primitiveIndex = primitiveIndex;
                    inst.jointMatrixOffset = runtimeMesh.jointMatrixOffset;
                    inst.bIsAllocated = 1;
                    for (int32_t i = 0; i < swapchain->imageCount; ++i) {
                        memcpy(static_cast<char*>(instanceBuffers[i].allocationInfo.pMappedData) + sizeof(Instance) * instanceEntry.index, &inst, sizeof(Instance));
                    }
                }
            }
        }

        if (node.jointMatrixIndex != ~0u) {
            glm::mat4 jointMatrix = node.cachedWorldTransform * node.inverseBindMatrix;
            const uint32_t jointMatrixFinalIndex = node.jointMatrixIndex + runtimeMesh.jointMatrixOffset;
            for (int32_t i = 0; i < swapchain->imageCount; ++i) {
                memcpy(static_cast<char*>(jointMatrixBuffers[i].allocationInfo.pMappedData) + jointMatrixFinalIndex * sizeof(Model), &jointMatrix, sizeof(Model));
            }
        }
    }
}

void ModelLoading::UpdateRuntimeMesh(RuntimeMesh& runtimeMesh, const AllocatedBuffer& modelBuffer, const AllocatedBuffer& jointMatrixBuffer)
{
    for (RuntimeNode& node : runtimeMesh.nodes) {
        if (node.meshIndex != ~0u) {
            char* ptr = static_cast<char*>(modelBuffer.allocationInfo.pMappedData) + node.modelMatrixHandle.index * sizeof(Model) + offsetof(Model, modelMatrix);
            memcpy(ptr, &node.cachedWorldTransform, sizeof(node.cachedWorldTransform));
        }

        if (node.jointMatrixIndex != ~0u) {
            glm::mat4 jointMatrix = node.cachedWorldTransform * node.inverseBindMatrix;
            uint32_t jointMatrixFinalIndex = node.jointMatrixIndex + runtimeMesh.jointMatrixOffset;
            memcpy(static_cast<char*>(jointMatrixBuffer.allocationInfo.pMappedData) + jointMatrixFinalIndex * sizeof(Model) + offsetof(Model, modelMatrix), &jointMatrix, sizeof(jointMatrix));
        }
    }
}
}
