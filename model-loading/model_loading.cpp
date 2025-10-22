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
#include "render/model_loader.h"
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


    //
    {
        VkPushConstantRange renderPushConstantRange{};
        renderPushConstantRange.offset = 0;
        renderPushConstantRange.size = sizeof(BindlessAddressPushConstant);
        renderPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkPipelineLayoutCreateInfo renderPipelineLayoutCreateInfo{};
        renderPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        renderPipelineLayoutCreateInfo.setLayoutCount = 0;
        renderPipelineLayoutCreateInfo.pSetLayouts = nullptr;
        renderPipelineLayoutCreateInfo.pPushConstantRanges = &renderPushConstantRange;
        renderPipelineLayoutCreateInfo.pushConstantRangeCount = 1;

        // std::array layouts{resourceManager.getSceneDataLayout(), samplerDescriptorLayout->layout};
        // layoutInfo.setLayoutCount = 0;
        // layoutInfo.pSetLayouts = layouts.data();


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
                .stride = sizeof(VertexPosition),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
            {
                .binding = 1,
                .stride = sizeof(VertexProperty),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            }
        };

        const std::vector<VkVertexInputAttributeDescription> vertexAttributes{
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(VertexPosition, position),

            },
            {
                .location = 1,
                .binding = 1,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(VertexProperty, normal),

            },
            {
                .location = 2,
                .binding = 1,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(VertexProperty, tangent),

            },
            {
                .location = 3,
                .binding = 1,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(VertexProperty, color),

            },
            {
                .location = 4,
                .binding = 1,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(VertexProperty, uv),

            }
        };

        renderPipelineBuilder.setupVertexInput(vertexBindings, vertexAttributes);

        renderPipelineBuilder.setShaders(vertShader, fragShader);
        renderPipelineBuilder.setupInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        renderPipelineBuilder.setupRasterization(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
        renderPipelineBuilder.disableMultisampling();
        renderPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
        renderPipelineBuilder.setupRenderer({DRAW_IMAGE_FORMAT}, DEPTH_IMAGE_FORMAT);
        renderPipelineBuilder.setupPipelineLayout(renderPipelineLayout.handle);
        VkGraphicsPipelineCreateInfo pipelineCreateInfo = renderPipelineBuilder.generatePipelineCreateInfo();
        renderPipeline = VkResources::CreateGraphicsPipeline(vulkanContext.get(), pipelineCreateInfo);

        vkDestroyShaderModule(vulkanContext->device, vertShader, nullptr);
        vkDestroyShaderModule(vulkanContext->device, fragShader, nullptr);
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


    // todo: optimize so that it's GPU only, use a staging buffer to upload in production
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    bufferInfo.usage = VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT;
    bufferInfo.size = sizeof(VertexPosition) * MEGA_VERTEX_BUFFER_COUNT;
    megaVertexPositionBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT;
    bufferInfo.size = sizeof(VertexProperty) * MEGA_VERTEX_BUFFER_COUNT;
    megaVertexPropertyBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT;
    bufferInfo.size = sizeof(uint32_t) * MEGA_INDEX_BUFFER_COUNT;
    megaIndexBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(MaterialProperties) * MEGA_MATERIAL_BUFFER_COUNT;
    materialBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Primitive) * MEGA_PRIMITIVE_BUFFER_COUNT;
    primitiveBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);

    // todo: there should be N(FIF) of this, but for this test bed all resources are initialized well before render loop and are not modified.
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Model) * BINDLESS_MODEL_COUNT;
    modelBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);
    bufferInfo.usage = VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.size = sizeof(Instance) * BINDLESS_INSTANCE_COUNT;
    instanceBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);


    // Suzanne Model
    auto suzannePath = std::filesystem::path("../assets/Suzanne/glTF/Suzanne.gltf");
    ExtractedModel suzanne = modelLoader->LoadGltf(suzannePath);

    modelDatas.reserve(2);
    modelDatas.emplace_back();
    ModelData& suzanneModelData = modelDatas[0];

    suzanneModelData.name = suzannePath.filename().string();
    suzanneModelData.path = suzannePath;
    // todo: unify vertex buffers and use stride instead.
    size_t sizeVertexPos = suzanne.vertexPositions.size() * sizeof(VertexPosition);
    suzanneModelData.vertexPositionAllocation = vertexPositionBufferAllocator.allocate(sizeVertexPos);
    memcpy(static_cast<char*>(megaVertexPositionBuffer.allocationInfo.pMappedData) + suzanneModelData.vertexPositionAllocation.offset, suzanne.vertexPositions.data(), sizeVertexPos);
    size_t sizeVertexProp = suzanne.vertexProperties.size() * sizeof(VertexProperty);
    suzanneModelData.vertexPropertyAllocation = vertexPropertyBufferAllocator.allocate(sizeVertexProp);
    memcpy(static_cast<char*>(megaVertexPropertyBuffer.allocationInfo.pMappedData) + suzanneModelData.vertexPropertyAllocation.offset, suzanne.vertexProperties.data(), sizeVertexProp);
    size_t sizeIndices = suzanne.indices.size() * sizeof(uint32_t);
    suzanneModelData.indexAllocation = indexBufferAllocator.allocate(sizeIndices);
    memcpy(static_cast<char*>(megaIndexBuffer.allocationInfo.pMappedData) + suzanneModelData.indexAllocation.offset, suzanne.indices.data(), sizeIndices);
    size_t sizeMaterials = suzanne.materials.size() * sizeof(MaterialProperties);
    suzanneModelData.materialAllocation = materialBufferAllocator.allocate(sizeMaterials);
    memcpy(static_cast<char*>(materialBuffer.allocationInfo.pMappedData) + suzanneModelData.materialAllocation.offset, suzanne.materials.data(), sizeMaterials);
    uint32_t firstIndexCount = suzanneModelData.indexAllocation.offset / sizeof(uint32_t);
    uint32_t vertexOffsetCount = suzanneModelData.vertexPositionAllocation.offset / sizeof(VertexPosition);
    uint32_t materialOffsetCount = suzanneModelData.materialAllocation.offset / sizeof(MaterialProperties);
    for (auto& primitive : suzanne.primitives) {
        primitive.firstIndex += firstIndexCount;
        primitive.vertexOffset += vertexOffsetCount;
        primitive.materialIndex += materialOffsetCount;
    }

    size_t sizePrimitives = suzanne.primitives.size() * sizeof(Primitive);
    suzanneModelData.primitiveAllocation = primitiveBufferAllocator.allocate(sizePrimitives);
    memcpy(static_cast<char*>(primitiveBuffer.allocationInfo.pMappedData) + suzanneModelData.primitiveAllocation.offset, suzanne.primitives.data(), sizePrimitives);

    uint32_t primitiveOffsetCount = suzanneModelData.primitiveAllocation.offset / sizeof(Primitive);
    suzanneModelData.meshes = std::move(suzanne.meshes);
    for (auto& mesh : suzanneModelData.meshes) {
        for (auto& primitiveIndex : mesh.primitiveIndices) {
            primitiveIndex += primitiveOffsetCount;
        }
    }


    // Box Model
    auto boxPath = std::filesystem::path("../assets/BoxTextured.glb");
    ExtractedModel boxTextured = modelLoader->LoadGltf(boxPath);

    modelDatas.emplace_back();
    ModelData& boxModelData = modelDatas[1];

    boxModelData.name = boxPath.filename().string();
    boxModelData.path = boxPath;
    sizeVertexPos = boxTextured.vertexPositions.size() * sizeof(VertexPosition);
    boxModelData.vertexPositionAllocation = vertexPositionBufferAllocator.allocate(sizeVertexPos);
    memcpy(static_cast<char*>(megaVertexPositionBuffer.allocationInfo.pMappedData) + boxModelData.vertexPositionAllocation.offset, boxTextured.vertexPositions.data(), sizeVertexPos);
    sizeVertexProp = boxTextured.vertexProperties.size() * sizeof(VertexProperty);
    boxModelData.vertexPropertyAllocation = vertexPropertyBufferAllocator.allocate(sizeVertexProp);
    memcpy(static_cast<char*>(megaVertexPropertyBuffer.allocationInfo.pMappedData) + boxModelData.vertexPropertyAllocation.offset, boxTextured.vertexProperties.data(), sizeVertexProp);
    sizeIndices = boxTextured.indices.size() * sizeof(uint32_t);
    boxModelData.indexAllocation = indexBufferAllocator.allocate(sizeIndices);
    memcpy(static_cast<char*>(megaIndexBuffer.allocationInfo.pMappedData) + boxModelData.indexAllocation.offset, boxTextured.indices.data(), sizeIndices);
    sizeMaterials = boxTextured.materials.size() * sizeof(MaterialProperties);
    boxModelData.materialAllocation = materialBufferAllocator.allocate(sizeMaterials);
    memcpy(static_cast<char*>(materialBuffer.allocationInfo.pMappedData) + boxModelData.materialAllocation.offset, boxTextured.materials.data(), sizeMaterials);
    firstIndexCount = boxModelData.indexAllocation.offset / sizeof(uint32_t);
    vertexOffsetCount = boxModelData.vertexPositionAllocation.offset / sizeof(VertexPosition);
    materialOffsetCount = boxModelData.materialAllocation.offset / sizeof(MaterialProperties);
    for (auto& primitive : boxTextured.primitives) {
        primitive.firstIndex += firstIndexCount;
        primitive.vertexOffset += vertexOffsetCount;
        primitive.materialIndex += materialOffsetCount;
    }

    sizePrimitives = boxTextured.primitives.size() * sizeof(Primitive);
    boxModelData.primitiveAllocation = primitiveBufferAllocator.allocate(sizePrimitives);
    memcpy(static_cast<char*>(primitiveBuffer.allocationInfo.pMappedData) + boxModelData.primitiveAllocation.offset, boxTextured.primitives.data(), sizePrimitives);

    primitiveOffsetCount = boxModelData.primitiveAllocation.offset / sizeof(Primitive);
    boxModelData.meshes = std::move(boxTextured.meshes);
    for (auto& mesh : boxModelData.meshes) {
        for (auto& primitiveIndex : mesh.primitiveIndices) {
            primitiveIndex += primitiveOffsetCount;
        }
    }

    // 3x3 on X+Y axis (model index 0->8)
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            glm::mat4 boxPosition = glm::translate(glm::mat4(1.0f), glm::vec3(x * 2.0f, y * 2.0f, 0.0f));
            boxPosition = glm::rotate(boxPosition, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            Model m{
                .modelMatrix = boxPosition,
                .prevModelMatrix = boxPosition,
                .flags = {1.0f, 1.0f, 1.0f, 1.0f}
            };

            size_t offset = (y * 3 + x) * sizeof(Model);
            memcpy(static_cast<char*>(modelBuffer.allocationInfo.pMappedData) + offset, &m, sizeof(Model));
        }
    }

    // 3x3 on X+Y axis, 5 Z forward (model index 9->17)
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            glm::mat4 boxPosition = glm::translate(glm::mat4(1.0f), glm::vec3(x * 2.0f, y * 2.0f, 5.0f));
            boxPosition = glm::rotate(boxPosition, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 5.0f));

            Model m{
                .modelMatrix = boxPosition,
                .prevModelMatrix = boxPosition,
                .flags = {1.0f, 1.0f, 1.0f, 1.0f}
            };

            size_t offset = (y * 3 + x + 9) * sizeof(Model);
            memcpy(static_cast<char*>(modelBuffer.allocationInfo.pMappedData) + offset, &m, sizeof(Model));
        }
    }


    for (const MeshInformation& mesh : suzanneModelData.meshes) {
        for (auto primitiveIndex : mesh.primitiveIndices) {
            instances.emplace_back(primitiveIndex, 0, 1);
            instances.emplace_back(primitiveIndex, 1, 1);
            instances.emplace_back(primitiveIndex, 2, 1);
            instances.emplace_back(primitiveIndex, 3, 1);
            instances.emplace_back(primitiveIndex, 4, 1);
            instances.emplace_back(primitiveIndex, 5, 1);
            instances.emplace_back(primitiveIndex, 6, 1);
            instances.emplace_back(primitiveIndex, 7, 1);
            instances.emplace_back(primitiveIndex, 8, 1);
        }
    }
    for (const MeshInformation& mesh : boxModelData.meshes) {
        for (auto primitiveIndex : mesh.primitiveIndices) {
            instances.emplace_back(primitiveIndex, 9, 1);
            instances.emplace_back(primitiveIndex, 10, 1);
            instances.emplace_back(primitiveIndex, 11, 1);
            instances.emplace_back(primitiveIndex, 12, 1);
            instances.emplace_back(primitiveIndex, 13, 1);
            instances.emplace_back(primitiveIndex, 14, 1);
            instances.emplace_back(primitiveIndex, 15, 1);
            instances.emplace_back(primitiveIndex, 16, 1);
            instances.emplace_back(primitiveIndex, 17, 1);
        }
    }
    memcpy(instanceBuffer.allocationInfo.pMappedData, instances.data(), sizeof(Instance) * instances.size());

    // This will be done in a compute shader in the future.
    {

        // A CPU copy of the primitive buffer to mimic what the drawCull pass would look like
        std::vector<Primitive> cpuPrimitives;
        cpuPrimitives.push_back(suzanne.primitives[0]);
        cpuPrimitives.push_back(boxTextured.primitives[0]);

        std::vector<VkDrawIndexedIndirectCommand> indirectCommands;
        for (auto& instance : instances) {
            // We use extracted model here, but the data is readily available in the primitve buffer for the compute shader. Access will also be different.
            ExtractedModel* _m = &suzanne;
            indirectCommands.push_back({
                cpuPrimitives[instance.primitiveIndex].indexCount,
                1, // instanceCount is always 1
                cpuPrimitives[instance.primitiveIndex].firstIndex,
                cpuPrimitives[instance.primitiveIndex].vertexOffset,
                instance.modelIndex
            });
            indirectCommandCount++;
        }

        const size_t indirectBufferSize = sizeof(VkDrawIndexedIndirectCommand) * indirectCommandCount;

        bufferInfo.usage = VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT;
        bufferInfo.size = indirectBufferSize;
        indexedIndirectBuffer = VkResources::CreateAllocatedBuffer(vulkanContext.get(), bufferInfo, vmaAllocInfo);

        memcpy(indexedIndirectBuffer.allocationInfo.pMappedData, indirectCommands.data(), indirectBufferSize);
    }

    CreateResources();
}

void ModelLoading::Run()
{
    Input::Input& input = Input::Input::Get();
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

        if (bSwapchainOutdated) {
            vkDeviceWaitIdle(vulkanContext->device);

            int32_t w, h;
            SDL_GetWindowSize(window, &w, &h);

            swapchain->Recreate(w, h);
            Input::Input::Get().UpdateWindowExtent(swapchain->extent.width, swapchain->extent.height);
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
    Input::Input& input = Input::Input::Get();

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

        if (input.IsKeyPressed(Input::Key::G)) {
            LOG_INFO("G is pressed");
        }
        if (input.IsKeyReleased(Input::Key::G)) {
            LOG_INFO("G is released");
        }
        if (ImGui::Begin("Main")) {
            ImGui::Text("Hello!");
            ImGui::DragFloat3("Position", cameraPos);
            ImGui::DragFloat3("Look", cameraLook);
            ImGui::DragFloat3("Box Pos", boxPos);
        }


        ImGui::End();
        ImGui::Render();
    }

    // Warning: Synchronization issues present in this code
    glm::mat4 boxPosition = glm::translate(glm::mat4(1.0f), glm::vec3(boxPos[0], boxPos[1], boxPos[2]));
    boxPosition = glm::rotate(boxPosition, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    Model m{
        .modelMatrix = boxPosition,
        .prevModelMatrix = boxPosition,
        .flags = {1.0f, 1.0f, 1.0f, 1.0f}
    };
    memcpy(modelBuffer.allocationInfo.pMappedData, &m, sizeof(Model));

    //
    {
        // Transition 1
        {
            auto subresource = VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
            auto barrier = VkHelpers::ImageMemoryBarrier(
                renderTargets->drawImage.handle,
                subresource,
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
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

            // Dynamic States
            //  Viewport
            VkViewport viewport = VkHelpers::GenerateViewport(scaledRenderExtent[0], scaledRenderExtent[1]);
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            //  Scissor
            VkRect2D scissor = VkHelpers::GenerateScissor(scaledRenderExtent[0], scaledRenderExtent[1]);
            vkCmdSetScissor(cmd, 0, 1, &scissor);


            glm::mat4 view = glm::lookAt(
                glm::vec3(cameraPos[0], cameraPos[1], cameraPos[2]),
                glm::vec3(cameraLook[0], cameraLook[1], cameraLook[2]),
                WORLD_UP
            );

            glm::mat4 proj = glm::perspective(
                glm::radians(75.0f),
                static_cast<float>(scaledRenderExtent[0]) / static_cast<float>(scaledRenderExtent[1]),
                1000.0f,
                0.1f
            );

            BindlessAddressPushConstant pushData{
                proj * view,
                materialBuffer.address,
                primitiveBuffer.address,
                modelBuffer.address,
                instanceBuffer.address,
            };


            vkCmdPushConstants(cmd, renderPipelineLayout.handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BindlessAddressPushConstant), &pushData);


            const VkBuffer vertexBuffers[2] = {megaVertexPositionBuffer.handle, megaVertexPropertyBuffer.handle};
            constexpr VkDeviceSize vertexOffsets[2] = {0, 0};
            vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, vertexOffsets);
            vkCmdBindIndexBuffer(cmd, megaIndexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexedIndirect(cmd, indexedIndirectBuffer.handle, 0, indirectCommandCount, sizeof(VkDrawIndexedIndirectCommand));

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
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
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
                VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
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
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE,
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
}
