//
// Created by William on 2025-11-12.
//

#include "engine.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "crash-handling/crash_context.h"
#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"
#include "core/constants.h"
#include "core/time.h"
#include "crash-handling/logger_helpers.h"
#include "input/input.h"
#include "utils/utils.h"

#include "hot-reloading/game/game_state.h"
#include "render/resource_manager.h"

namespace HotReloading::Engine
{
void StubInit(Game::GameState* state)
{
    LOG_WARN("Game DLL not loaded - using stub GameInit");
}

void StubUpdate(Game::GameState* state, float deltaTime)
{
    LOG_WARN("Game DLL not loaded - using stub GameUpdate");
}

void StubShutdown(Game::GameState* state)
{
    LOG_WARN("Game DLL not loaded - using stub GameShutdown");
}

Engine* Engine::instance = nullptr;

Engine::Engine()
{
    instance = this;
};

Engine::~Engine() = default;

void Engine::Initialize()
{
    fmt::println("=== Hot Reloading ===");
    start = std::chrono::high_resolution_clock::now();

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/hot-reloading.log");

    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    window = SDL_CreateWindow(
        "Template",
        ::Core::DEFAULT_WINDOW_WIDTH,
        ::Core::DEFAULT_WINDOW_HEIGHT,
        window_flags);

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    int32_t w;
    int32_t h;
    SDL_GetWindowSize(window, &w, &h);
    Input::Get().Init(window, w, h);

    if (gameDll.Load("game/hot-reload-game.dll", "hot-reload-game_temp.dll")) {
        gameFunctions.gameInit = gameDll.GetFunction<GameInitFunc>("GameInit");
        gameFunctions.gameUpdate = gameDll.GetFunction<GameUpdateFunc>("GameUpdate");
        gameFunctions.gameShutdown = gameDll.GetFunction<GameShutdownFunc>("GameShutdown");
    }
    else {
        gameFunctions.Stub();
    }

    gameFunctions.gameInit(&gameState);
    renderThread.Initialize(&engineSynchronization, window, w, h);
    assetLoadingThread.Initialize(renderThread.GetVulkanContext(), renderThread.GetResourceManager());
}

void Engine::Run()
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
        constexpr auto gameWait = std::chrono::milliseconds(100);
        std::this_thread::sleep_for(gameWait);

        input.FrameReset();
        while (SDL_PollEvent(&e) != 0) {
            input.ProcessEvent(e);
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
            if (e.type == SDL_EVENT_WINDOW_MINIMIZED
                || e.type == SDL_EVENT_WINDOW_RESTORED
                || e.type == SDL_EVENT_WINDOW_MAXIMIZED
                || e.type == SDL_EVENT_WINDOW_RESIZED
                || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                bSwapchainOutdated |= true;
            }
        }

        if (exit) {
            renderThread.RequestShutdown();
            engineSynchronization.renderFrames.release();
            assetLoadingThread.RequestShutdown();
            break;
        }

        if (input.IsKeyPressed(Key::F5)) {
            if (gameDll.Reload()) {
                gameFunctions.gameInit = gameDll.GetFunction<GameInitFunc>("GameInit");
                gameFunctions.gameUpdate = gameDll.GetFunction<GameUpdateFunc>("GameUpdate");
                gameFunctions.gameShutdown = gameDll.GetFunction<GameShutdownFunc>("GameShutdown");
                LOG_INFO("Game lib was hot-reloaded");
            }
            else {
                gameFunctions.Stub();
                LOG_INFO("Game lib failed to be hot-reloaded");
            }
        }

        SDL_WindowFlags windowFlags = SDL_GetWindowFlags(window);
        input.UpdateFocus(windowFlags);
        time.Update();


        ::Game::FreeCamera freeCamera{{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f}};
        const glm::vec3 cameraPos = freeCamera.GetPosition();
        const glm::quat cameraRot = freeCamera.GetRotation();
        const glm::vec3 forward = freeCamera.GetForward();
        const glm::vec3 up = freeCamera.GetUp();

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + forward, up);


        rawSceneData.view = view;
        rawSceneData.cameraWorldPos = glm::vec4(cameraPos, 1.0f);
        rawSceneData.fovDegrees = glm::degrees(freeCamera.GetFov());
        rawSceneData.nearPlane = freeCamera.GetNearPlane();
        rawSceneData.farPlane = freeCamera.GetFarPlane();
        rawSceneData.deltaTime += time.GetDeltaTime();

        gameState.frame = gameFrame;
        assetLoadingThread.ResolveLoads(loadedModelEntryHandles, bufferAcquireOperations, imageAcquireOperations);

        AssetLoad::ModelEntryHandle loadedModelEntryHandle;
        while (loadedModelEntryHandles.Pop(loadedModelEntryHandle)) {
            auto index = loadedModelEntryHandle.index;
            LOG_INFO("Loaded model! {}", index);
        }

        gameFunctions.gameUpdate(&gameState, 0.0f);

        bool canTransmit = engineSynchronization.gameFrames.try_acquire();
        if (canTransmit) {
            uint64_t currentRenderFrame = renderFrame % ::Core::FRAMES_IN_FLIGHT;
            Renderer::FrameBuffer& currentFrameBuffer = engineSynchronization.frameBuffers[currentRenderFrame];
            PrepareFrameDataForRender(currentFrameBuffer);
            renderFrame++;
            engineSynchronization.renderFrames.release();
        }

        gameFrame++;
    }
}

void Engine::Cleanup()
{
    renderThread.Join();
    assetLoadingThread.Join();

    gameFunctions.gameShutdown(&gameState);
    gameDll.Unload();

    SDL_DestroyWindow(window);
}

void Engine::PrepareFrameDataForRender(Renderer::FrameBuffer& frameBuffer)
{
    frameBuffer.modelMatrixOperations.insert(frameBuffer.modelMatrixOperations.end(), modelMatrixOperations.begin(), modelMatrixOperations.end());
    frameBuffer.instanceOperations.insert(frameBuffer.instanceOperations.end(), instanceOperations.begin(), instanceOperations.end());
    frameBuffer.jointMatrixOperations.insert(frameBuffer.jointMatrixOperations.end(), jointMatrixOperations.begin(), jointMatrixOperations.end());
    frameBuffer.bufferAcquireOperations.insert(frameBuffer.bufferAcquireOperations.end(), bufferAcquireOperations.begin(), bufferAcquireOperations.end());
    frameBuffer.imageAcquireOperations.insert(frameBuffer.imageAcquireOperations.end(), imageAcquireOperations.begin(), imageAcquireOperations.end());

    modelMatrixOperations.clear();
    instanceOperations.clear();
    jointMatrixOperations.clear();
    bufferAcquireOperations.clear();
    imageAcquireOperations.clear();

    Time& time = Time::Get();
    rawSceneData.timeElapsed = time.GetTime();

    frameBuffer.rawSceneData = rawSceneData;
    frameBuffer.currentFrame = gameFrame;
    frameBuffer.bRequireSwapchainRecreate = bSwapchainOutdated;

    rawSceneData.prevView = rawSceneData.view;
    rawSceneData.prevCameraWorldPos = rawSceneData.cameraWorldPos;
    rawSceneData.prevFovDegrees = rawSceneData.fovDegrees;
    rawSceneData.prevNearPlane = rawSceneData.nearPlane;
    rawSceneData.prevFarPlane = rawSceneData.farPlane;
    rawSceneData.deltaTime = 0;

    bSwapchainOutdated = false;
}

AssetLoad::RequestLoad Engine::RequestModelLoad(const char* path)
{
    return assetLoadingThread.RequestLoad(path);
}

AssetLoad::RuntimeMeshHandle Engine::GenerateModel(AssetLoad::ModelEntryHandle modelEntryHandle, const Transform& topLevelTransform)
{
    Renderer::ModelData* modelData = assetLoadingThread.GetModelData(modelEntryHandle);
    if (!modelData) { return AssetLoad::RuntimeMeshHandle::Invalid; }
    AssetLoad::RuntimeMeshHandle newRuntimeMeshHandle = runtimeMeshes.Add();
    if (!newRuntimeMeshHandle.IsValid()) { return AssetLoad::RuntimeMeshHandle::Invalid; }
    AssetLoad::RuntimeMesh& rm = *runtimeMeshes.Get(newRuntimeMeshHandle);

    Renderer::ResourceManager* resourceManager = renderThread.GetResourceManager();
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

    UpdateRuntimeMesh(newRuntimeMeshHandle, topLevelTransform);
    return newRuntimeMeshHandle;
}

bool Engine::UpdateRuntimeMesh(AssetLoad::RuntimeMeshHandle runtimeMeshHandle, const Transform& topLevelTransform)
{
    AssetLoad::RuntimeMesh* runtimeMesh = runtimeMeshes.Get(runtimeMeshHandle);
    if (!runtimeMesh) { return false; }

    runtimeMesh->transform = topLevelTransform;
    UpdateTransforms(runtimeMesh);

    for (Renderer::RuntimeNode& node : runtimeMesh->nodes) {
        if (node.meshIndex != ~0u) {
            modelMatrixOperations.push_back({node.modelMatrixHandle.index, node.cachedWorldTransform});
        }

        if (node.jointMatrixIndex != ~0u) {
            glm::mat4 jointMatrix = node.cachedWorldTransform * node.inverseBindMatrix;
            uint32_t jointMatrixFinalIndex = node.jointMatrixIndex + runtimeMesh->jointMatrixOffset;
            jointMatrixOperations.push_back({jointMatrixFinalIndex, jointMatrix});
        }
    }

    return true;
}

void Engine::UpdateTransforms(AssetLoad::RuntimeMesh* runtimeMesh)
{
    glm::mat4 baseTopLevel = runtimeMesh->transform.GetMatrix();

    // Nodes are sorted
    for (Renderer::RuntimeNode& rn : runtimeMesh->nodes) {
        glm::mat4 localTransform = rn.transform.GetMatrix();

        if (rn.parent == ~0u) {
            rn.cachedWorldTransform = baseTopLevel * localTransform;
        }
        else {
            rn.cachedWorldTransform = runtimeMesh->nodes[rn.parent].cachedWorldTransform * localTransform;
        }
    }
}
} // HotReloading::Engine
