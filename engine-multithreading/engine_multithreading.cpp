//
// Created by William on 2025-10-31.
//

#include "engine_multithreading.h"

#include <SDL3/SDL.h>

#include "core/constants.h"
#include "core/time.h"
#include "crash-handling/crash_handler.h"
#include "input/input.h"
#include "render/resource_manager.h"
#include "render/vk_imgui_wrapper.h"
#include "utils/utils.h"
#include "utils/world_constants.h"

EngineMultithreading::EngineMultithreading() = default;

EngineMultithreading::~EngineMultithreading() = default;

void EngineMultithreading::Initialize()
{
    start = std::chrono::high_resolution_clock::now();
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        return;
    }

    constexpr bool fullscreen = false;
    if (fullscreen) {
        SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode* displayMode = SDL_GetCurrentDisplayMode(primaryDisplay);
        constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN;
        window = SDL_CreateWindow(
            "Engine Multithreading Tests",
            displayMode->w,
            displayMode->h,
            window_flags);
    }
    else {
        constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
        window = SDL_CreateWindow(
            "Engine Multithreading Tests",
            Core::DEFAULT_WINDOW_WIDTH,
            Core::DEFAULT_WINDOW_HEIGHT,
            window_flags);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    SDL_ShowWindow(window);
    int32_t w;
    int32_t h;
    SDL_GetWindowSize(window, &w, &h);
    renderThread.Initialize(this, window, w, h);
    assetLoadingThread.Initialize(renderThread.GetVulkanContext(), renderThread.GetResourceManager());

    Input::Get().Init(window, w, h);

    loadedModelsToAcquire.reserve(10);
}

void EngineMultithreading::Run()
{
    Utils::SetThreadName("GameThread");

    renderThread.Start();
    assetLoadingThread.Start();

    Input& input = Input::Input::Get();
    Time& time = Time::Get();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    LOG_INFO("Engine Multithreading initialized in {:.3f}s", duration.count() / 1000000.0);

    SDL_Event e;
    bool exit = false;
    while (true) {
        input.FrameReset();
        while (SDL_PollEvent(&e) != 0) {
            input.ProcessEvent(e);
            Renderer::ImguiWrapper::HandleInput(e);
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
        }

        input.UpdateFocus(SDL_GetWindowFlags(window));
        time.Update();

        if (exit) {
            renderThread.RequestShutdown();
            renderFrames.release();
            assetLoadingThread.RequestShutdown();
            break;
        }


        assetLoadingThread.ResolveLoads(loadedModelsToAcquire);

        bool canTransmit = gameFrames.try_acquire();
        if (canTransmit) {
            uint64_t currentRenderFrame = renderFrame % Core::FRAMES_IN_FLIGHT;
            PrepareFrameDataForRender(currentRenderFrame);
            renderFrame++;
            renderFrames.release();
        }


        ThreadMain();
        gameFrame++;
    }
}

void EngineMultithreading::ThreadMain()
{
    auto gameThreadTime = std::chrono::milliseconds(5);
    std::this_thread::sleep_for(gameThreadTime);

    Input& input = Input::Input::Get();
    Time& time = Time::Get();
    const float deltaTime = time.GetDeltaTime();
    const float timeElapsed = time.GetTime();

    freeCamera.Update(deltaTime);

    if (input.IsKeyPressed(Key::NUM_1)) {
        auto suzannePath = std::filesystem::path("../assets/Suzanne/glTF/Suzanne.gltf");
        assetLoadingThread.RequestLoad(suzannePath, [this](Renderer::ModelEntryHandle handle) {
            if (handle == Renderer::ModelEntryHandle::Invalid) {
                LOG_ERROR("Failed to load 'Suzanne'");
                return;
            }

            LOG_INFO("Successfully loaded 'Suzanne'");
            suzanneModelEntryHandle = handle;

            // Model is loaded and ready to use
            // Store handle for rendering
            // loadedModels.push_back(handle);

            // Or immediately spawn an entity with it
            // SpawnEntity(handle);
        });
    }
    if (input.IsKeyPressed(Key::NUM_2)) {
        auto structurePath = std::filesystem::path("../assets/structure.glb");
        assetLoadingThread.RequestLoad(structurePath, [this](Renderer::ModelEntryHandle handle) {
            if (handle == Renderer::ModelEntryHandle::Invalid) {
                LOG_ERROR("Failed to load 'Structure model'");
                return;
            }

            LOG_INFO("Successfully loaded 'Structure model'");

            structureModelEntryHandle = handle;
        });
    }
    if (input.IsKeyPressed(Key::NUM_3)) {
        auto riggedFigurePath = std::filesystem::path("../assets/RiggedFigure.glb");

        assetLoadingThread.RequestLoad(riggedFigurePath, [this](Renderer::ModelEntryHandle handle) {
            if (handle == Renderer::ModelEntryHandle::Invalid) {
                LOG_ERROR("Failed to load 'Rigged Figure'");
                return;
            }

            LOG_INFO("Successfully loaded 'Rigged Figure'");

            riggedFigureModelEntryHandle = handle;
        });
    }
    if (input.IsKeyPressed(Key::NUM_4)) {
        auto boxPath = std::filesystem::path("../assets/BoxTextured.glb");

        assetLoadingThread.RequestLoad(boxPath, [this](Renderer::ModelEntryHandle handle) {
            if (handle == Renderer::ModelEntryHandle::Invalid) {
                LOG_ERROR("Failed to load 'Textured Box model'");
                return;
            }

            LOG_INFO("Successfully loaded 'Textured Box'");

            texturedBoxModelEntryHandle = handle;
        });
    }

    if (input.IsKeyPressed(Key::Q)) {
        if (suzanneModelEntryHandle != Renderer::ModelEntryHandle::Invalid && suzanneRuntimeMesh.modelEntryHandle == Renderer::ModelEntryHandle::Invalid) {
            suzanneRuntimeMesh = GenerateModel(suzanneModelEntryHandle, Transform::Identity);
            UpdateTransforms(suzanneRuntimeMesh);
            LOG_INFO("Sent Suzanne to be drawn by GPU");
        }
    }
    if (input.IsKeyPressed(Key::W)) {
        if (structureModelEntryHandle != Renderer::ModelEntryHandle::Invalid && structureRuntimeMesh.modelEntryHandle == Renderer::ModelEntryHandle::Invalid) {
            structureRuntimeMesh = GenerateModel(structureModelEntryHandle, Transform::Identity);
            UpdateTransforms(structureRuntimeMesh);
            LOG_INFO("Sent Structure to be drawn by GPU");
        }
    }

    static bool bStartMoving = false;
    if (input.IsKeyPressed(Key::A)) {
        if (suzanneRuntimeMesh.modelEntryHandle != Renderer::ModelEntryHandle::Invalid) {
            bStartMoving = true;
        }
    }

    if (bStartMoving) {
        static float _time = 0.0f;
        _time += deltaTime;
        float yOffset = 1.0f * sinf(_time * 0.5f);
        suzanneRuntimeMesh.transform.translation = {0.0f, yOffset, 0.0f};
        UpdateTransforms(suzanneRuntimeMesh);
    }

    const glm::vec3 cameraPos = freeCamera.GetPosition();
    const glm::quat cameraRot = freeCamera.GetRotation();
    const glm::vec3 forward = freeCamera.GetForward();
    const glm::vec3 up = freeCamera.GetUp();

    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + forward, up);

    rawSceneData.prevView = rawSceneData.view;
    rawSceneData.view = view;
    rawSceneData.prevCameraWorldPos = rawSceneData.cameraWorldPos;
    rawSceneData.cameraWorldPos = glm::vec4(cameraPos, 1.0f);
    rawSceneData.fovDegrees = glm::degrees(freeCamera.GetFov());
    rawSceneData.nearPlane = freeCamera.GetNearPlane();
    rawSceneData.farPlane = freeCamera.GetFarPlane();
    rawSceneData.timeElapsed = timeElapsed;
    rawSceneData.deltaTime = deltaTime;
}

void EngineMultithreading::Cleanup()
{
    renderThread.Join();
    assetLoadingThread.Join();

    SDL_DestroyWindow(window);
}

void EngineMultithreading::PrepareFrameDataForRender(uint64_t currentRenderFrame)
{
    Renderer::FrameBuffer& currentFrameBuffer = frameBuffers[currentRenderFrame];

    for (Renderer::ModelEntryHandle loadedModel : loadedModelsToAcquire) {
        if (Renderer::AcquireOperations* modelAcquires = assetLoadingThread.GetModelAcquires(loadedModel)) {
            currentFrameBuffer.bufferAcquireOperations.insert(
                currentFrameBuffer.bufferAcquireOperations.end(),
                modelAcquires->bufferAcquireOps.begin(),
                modelAcquires->bufferAcquireOps.end()
            );
            modelAcquires->bufferAcquireOps.clear();

            currentFrameBuffer.imageAcquireOperations.insert(
                currentFrameBuffer.imageAcquireOperations.end(),
                modelAcquires->imageAcquireOps.begin(),
                modelAcquires->imageAcquireOps.end()
            );
            modelAcquires->imageAcquireOps.clear();

            modelAcquires->bRequiresAcquisition = false;
        }
    }

    loadedModelsToAcquire.clear();

    currentFrameBuffer.currentFrame = gameFrame;
    currentFrameBuffer.rawSceneData = rawSceneData;

    for (Renderer::InstanceOperation& instanceOp : instanceOperations) {
        currentFrameBuffer.instanceOperations.push_back(instanceOp);
    }
    instanceOperations.clear();

    UpdateRuntimeMesh(suzanneRuntimeMesh, currentFrameBuffer.modelMatrixOperations, currentFrameBuffer.jointMatrixOperations);
    UpdateRuntimeMesh(structureRuntimeMesh, currentFrameBuffer.modelMatrixOperations, currentFrameBuffer.jointMatrixOperations);
    UpdateRuntimeMesh(riggedFigureRuntimeMesh, currentFrameBuffer.modelMatrixOperations, currentFrameBuffer.jointMatrixOperations);
    UpdateRuntimeMesh(texturedBoxRuntimeMesh, currentFrameBuffer.modelMatrixOperations, currentFrameBuffer.jointMatrixOperations);
}


Renderer::RuntimeMesh EngineMultithreading::GenerateModel(Renderer::ModelEntryHandle modelEntryHandle, const Transform& topLevelTransform)
{
    Renderer::ResourceManager* resourceManager = renderThread.GetResourceManager();
    Renderer::RuntimeMesh rm{};

    Renderer::ModelData* modelData = assetLoadingThread.GetModelData(modelEntryHandle);
    if (!modelData) { return rm; }

    rm.transform = topLevelTransform;
    rm.nodes.reserve(modelData->nodes.size());
    rm.nodeRemap = modelData->nodeRemap;

    size_t jointMatrixCount = modelData->inverseBindMatrices.size();
    bool bHasSkinning = jointMatrixCount > 0;
    if (bHasSkinning) {
        rm.jointMatrixAllocation = resourceManager->jointMatrixAllocator.allocate(jointMatrixCount * sizeof(Renderer::Model));
        rm.jointMatrixOffset = rm.jointMatrixAllocation.offset / sizeof(uint32_t);
    }

    rm.modelEntryHandle = modelEntryHandle;
    for (const Renderer::Node& n : modelData->nodes) {
        rm.nodes.emplace_back(n);
        Renderer::RuntimeNode& rn = rm.nodes.back();
        if (n.inverseBindIndex != ~0u) {
            rn.inverseBindMatrix = modelData->inverseBindMatrices[n.inverseBindIndex];
        }
    }

    for (Renderer::RuntimeNode& node : rm.nodes) {
        if (node.meshIndex != ~0u) {
            node.modelMatrixHandle = resourceManager->modelMatrixAllocator.Add();

            for (uint32_t primitiveIndex : modelData->meshes[node.meshIndex].primitiveIndices) {
                Renderer::InstanceEntryHandle instanceEntry = resourceManager->instanceEntryAllocator.Add();
                node.instanceEntryHandles.push_back(instanceEntry);

                Renderer::Instance inst;
                inst.modelIndex = node.modelMatrixHandle.index;
                inst.primitiveIndex = primitiveIndex;
                inst.jointMatrixOffset = rm.jointMatrixOffset;
                inst.bIsAllocated = 1;

                instanceOperations.push_back({instanceEntry.index, inst});
            }
        }
    }

    UpdateTransforms(rm);
    return rm;
}

void EngineMultithreading::UpdateTransforms(Renderer::RuntimeMesh& runtimeMesh)
{
    glm::mat4 baseTopLevel = runtimeMesh.transform.GetMatrix();

    // Nodes are sorted
    for (Renderer::RuntimeNode& rn : runtimeMesh.nodes) {
        glm::mat4 localTransform = rn.transform.GetMatrix();

        if (rn.parent == ~0u) {
            rn.cachedWorldTransform = baseTopLevel * localTransform;
        }
        else {
            rn.cachedWorldTransform = runtimeMesh.nodes[rn.parent].cachedWorldTransform * localTransform;
        }
    }

    runtimeMesh.bNeedToSendToRender = true;
}

void EngineMultithreading::UpdateRuntimeMesh(Renderer::RuntimeMesh& runtimeMesh,
                                             std::vector<Renderer::ModelMatrixOperation>& modelMatrixOperations,
                                             std::vector<Renderer::JointMatrixOperation>& jointMatrixOperations)
{
    if (!runtimeMesh.bNeedToSendToRender) {
        return;
    }

    for (Renderer::RuntimeNode& node : runtimeMesh.nodes) {
        if (node.meshIndex != ~0u) {
            modelMatrixOperations.push_back({node.modelMatrixHandle.index, node.cachedWorldTransform});
        }

        if (node.jointMatrixIndex != ~0u) {
            glm::mat4 jointMatrix = node.cachedWorldTransform * node.inverseBindMatrix;
            uint32_t jointMatrixFinalIndex = node.jointMatrixIndex + runtimeMesh.jointMatrixOffset;
            jointMatrixOperations.push_back({jointMatrixFinalIndex, jointMatrix});
        }
    }

    runtimeMesh.bNeedToSendToRender = false;
}

void EngineMultithreading::DeleteRuntimeMesh(Renderer::RuntimeMesh& runtimeMesh, std::vector<Renderer::InstanceOperation>& instanceOperations)
{
    Renderer::ResourceManager* resourceManager = renderThread.GetResourceManager();
    for (Renderer::RuntimeNode& node : runtimeMesh.nodes) {
        if (node.meshIndex != ~0u) {
            resourceManager->modelMatrixAllocator.Remove(node.modelMatrixHandle);
            for (Renderer::InstanceEntryHandle instanceEntryHandle : node.instanceEntryHandles) {
                instanceOperations.push_back({instanceEntryHandle.index, {}});
                resourceManager->instanceEntryAllocator.Remove(instanceEntryHandle);
            }
        }
    }

    resourceManager->jointMatrixAllocator.free(runtimeMesh.jointMatrixAllocation);
}
