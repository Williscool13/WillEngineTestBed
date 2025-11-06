//
// Created by William on 2025-10-31.
//

#include "engine_multithreading.h"

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
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        return;
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    window = SDL_CreateWindow(
        "Engine Multithreading Tests",
        Core::DEFAULT_WINDOW_WIDTH,
        Core::DEFAULT_WINDOW_HEIGHT,
        window_flags);

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    renderThread.Initialize(this, window, Core::DEFAULT_WINDOW_WIDTH, Core::DEFAULT_WINDOW_HEIGHT);
    assetLoadingThread.Initialize(renderThread.GetVulkanContext(), renderThread.GetResourceManager());

    Input::Get().Init(window, Core::DEFAULT_WINDOW_WIDTH, Core::DEFAULT_WINDOW_HEIGHT);
}

void EngineMultithreading::ThreadMain()
{
    Input& input = Input::Input::Get();
    Time& time = Time::Get();
    const float deltaTime = time.GetDeltaTime();
    const float timeElapsed = time.GetTime();


    uint32_t currentFrameInFlight = frameNumber % Core::FRAMES_IN_FLIGHT;
    Renderer::FrameBuffer& currentFrameBuffer = frameBuffers[currentFrameInFlight];

    if (input.IsKeyPressed(Key::NUM_1)) {
        auto suzannePath = std::filesystem::path("../assets/Suzanne/glTF/Suzanne.gltf");
        assetLoadingThread.RequestLoad(suzannePath, [this](Renderer::ModelEntryHandle handle) {
            if (handle == Renderer::ModelEntryHandle::Invalid) {
                LOG_ERROR("Failed to load Suzanne model");
                return;
            }

            LOG_INFO("Successfully to load Suzanne model");
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
                LOG_ERROR("Failed to load Structure model");
                return;
            }

            LOG_INFO("Successfully to load Structure model");

            structureModelEntryHandle = handle;
        });
    }
    if (input.IsKeyPressed(Key::NUM_3)) {
        auto riggedFigurePath = std::filesystem::path("../assets/RiggedFigure.glb");

        assetLoadingThread.RequestLoad(riggedFigurePath, [this](Renderer::ModelEntryHandle handle) {
            if (handle == Renderer::ModelEntryHandle::Invalid) {
                LOG_ERROR("Failed to load Rigged Figure model");
                return;
            }

            LOG_INFO("Successfully to load Rigged Figure model");

            riggedFigureModelEntryHandle = handle;
        });
    }
    if (input.IsKeyPressed(Key::NUM_4)) {
        auto boxPath = std::filesystem::path("../assets/BoxTextured.glb");

        assetLoadingThread.RequestLoad(boxPath, [this](Renderer::ModelEntryHandle handle) {
            if (handle == Renderer::ModelEntryHandle::Invalid) {
                LOG_ERROR("Failed to load Textured Box model");
                return;
            }

            LOG_INFO("Successfully to load Textured Box model");

            texturedBoxModelEntryHandle = handle;
        });
    }

    if (input.IsKeyPressed(Key::Q)) {
        if (suzanneModelEntryHandle != Renderer::ModelEntryHandle::Invalid && suzanneRuntimeMesh.modelEntryHandle == Renderer::ModelEntryHandle::Invalid) {
            suzanneRuntimeMesh = GenerateModel(suzanneModelEntryHandle, Transform::Identity);
            UpdateTransforms(suzanneRuntimeMesh.nodes, suzanneRuntimeMesh.transform);
            InitialUploadRuntimeMesh(suzanneRuntimeMesh, currentFrameBuffer.modelMatrixOperations, currentFrameBuffer.instanceOperations, currentFrameBuffer.jointMatrixOperations);
            LOG_INFO("Sent upload command to the GPU");
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
        UpdateTransforms(suzanneRuntimeMesh.nodes, suzanneRuntimeMesh.transform);
        UpdateRuntimeMesh(suzanneRuntimeMesh, currentFrameBuffer.modelMatrixOperations, currentFrameBuffer.instanceOperations, currentFrameBuffer.jointMatrixOperations);
    }

    constexpr float cameraPos[3] = {0, 0, 2};
    constexpr float cameraLook[3] = {0, 0, 0};
    glm::mat4 view = glm::lookAt(
        glm::vec3(cameraPos[0], cameraPos[1], cameraPos[2]),
        glm::vec3(cameraLook[0], cameraLook[1], cameraLook[2]),
        WORLD_UP);

    rawSceneData.prevView = rawSceneData.view;
    rawSceneData.view = view;
    rawSceneData.prevCameraWorldPos = rawSceneData.cameraWorldPos;
    rawSceneData.cameraWorldPos = {cameraPos[0], cameraPos[1], cameraPos[2]};
    rawSceneData.fovDegrees = 75.0f;
    rawSceneData.nearPlane = 0.1f;
    rawSceneData.farPlane = 1000.0f;
    rawSceneData.timeElapsed = timeElapsed;
    rawSceneData.deltaTime = deltaTime;

    currentFrameBuffer.currentFrame = frameNumber;
    currentFrameBuffer.rawSceneData = rawSceneData;
}

void EngineMultithreading::Run()
{
    Utils::SetThreadName("GameThread");

    renderThread.Start();
    assetLoadingThread.Start();

    Input& input = Input::Input::Get();
    Time& time = Time::Get();

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


        gameFrames.acquire();
        //
        {
            //LOG_INFO("[Game Thread] Processed frame {}", frameNumber);
            assetLoadingThread.ResolveLoads();

            ThreadMain();
        }

        renderFrames.release();
        frameNumber++;
    }

    renderThread.Join();
    assetLoadingThread.Join();
}

void EngineMultithreading::Cleanup()
{
    SDL_DestroyWindow(window);
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

    return rm;
}

void EngineMultithreading::UpdateTransforms(std::vector<Renderer::RuntimeNode>& runtimeNodes, const Transform& topLevelTransform)
{
    glm::mat4 baseTopLevel = topLevelTransform.GetMatrix();

    // Nodes are sorted
    for (Renderer::RuntimeNode& rn : runtimeNodes) {
        glm::mat4 localTransform = rn.transform.GetMatrix();

        if (rn.parent == ~0u) {
            rn.cachedWorldTransform = baseTopLevel * localTransform;
        }
        else {
            rn.cachedWorldTransform = runtimeNodes[rn.parent].cachedWorldTransform * localTransform;
        }
    }
}

void EngineMultithreading::InitialUploadRuntimeMesh(Renderer::RuntimeMesh& runtimeMesh,
                                                    std::vector<Renderer::ModelMatrixOperation>& modelMatrixOperations,
                                                    std::vector<Renderer::InstanceOperation>& instanceOperations,
                                                    std::vector<Renderer::JointMatrixOperation>& jointMatrixOperations)
{
    Renderer::ResourceManager* resourceManager = renderThread.GetResourceManager();
    for (Renderer::RuntimeNode& node : runtimeMesh.nodes) {
        if (node.meshIndex != ~0u) {
            node.modelMatrixHandle = resourceManager->modelMatrixAllocator.Add();
            modelMatrixOperations.push_back({node.modelMatrixHandle.index, node.cachedWorldTransform});

            if (Renderer::ModelData* modelData = assetLoadingThread.GetModelData(runtimeMesh.modelEntryHandle)) {
                for (uint32_t primitiveIndex : modelData->meshes[node.meshIndex].primitiveIndices) {
                    Renderer::InstanceEntryHandle instanceEntry = resourceManager->instanceEntryAllocator.Add();
                    node.instanceEntryHandles.push_back(instanceEntry);

                    Renderer::Instance inst;
                    inst.modelIndex = node.modelMatrixHandle.index;
                    inst.primitiveIndex = primitiveIndex;
                    inst.jointMatrixOffset = runtimeMesh.jointMatrixOffset;
                    inst.bIsAllocated = 1;

                    instanceOperations.push_back({instanceEntry.index, inst});
                }
            }
        }

        if (node.jointMatrixIndex != ~0u) {
            glm::mat4 jointMatrix = node.cachedWorldTransform * node.inverseBindMatrix;
            const uint32_t jointMatrixFinalIndex = node.jointMatrixIndex + runtimeMesh.jointMatrixOffset;
            jointMatrixOperations.push_back({jointMatrixFinalIndex, jointMatrix});
        }
    }
}

void EngineMultithreading::UpdateRuntimeMesh(Renderer::RuntimeMesh& runtimeMesh,
                                             std::vector<Renderer::ModelMatrixOperation>& modelMatrixOperations,
                                             std::vector<Renderer::InstanceOperation>& instanceOperations,
                                             std::vector<Renderer::JointMatrixOperation>& jointMatrixOperations)
{
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
}

void EngineMultithreading::DeleteRuntimeMesh(Renderer::RuntimeMesh& runtimeMesh,
                                             std::vector<Renderer::ModelMatrixOperation>& modelMatrixOperations,
                                             std::vector<Renderer::InstanceOperation>& instanceOperations,
                                             std::vector<Renderer::JointMatrixOperation>& jointMatrixOperations)
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
}
