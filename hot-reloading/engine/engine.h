//
// Created by William on 2025-11-12.
//

#ifndef WILLENGINETESTBED_ENGINE_H
#define WILLENGINETESTBED_ENGINE_H
#include <SDL3/SDL.h>

#include <hot-reloading/engine/engine_api.h>

#include "core/data-structures/ring_buffer.h"
#include "game/camera/free_camera.h"
#include "hot-reloading/game/game_state.h"
#include "hot-reloading/render/render.h"
#include "hot-reloading/asset-load/asset_loading_thread.h"

namespace HotReloading::Engine
{
using GameInitFunc = void(*)(Game::GameState*);
using GameUpdateFunc = void(*)(Game::GameState*, float);
using GameShutdownFunc = void(*)(Game::GameState*);

void StubInit(Game::GameState* state);

void StubUpdate(Game::GameState* state, float deltaTime);

void StubShutdown(Game::GameState* state);

struct GameFunctions
{
    GameInitFunc gameInit;
    GameUpdateFunc gameUpdate;
    GameShutdownFunc gameShutdown;

    void Stub()
    {
        gameInit = StubInit;
        gameUpdate = StubUpdate;
        gameShutdown = StubShutdown;
    }
};


class ENGINE_API Engine
{
public:
    static Engine& Get()
    {
        return *instance;
    }

    Engine();

    ~Engine();

    void Initialize();

    void Run();

    void Cleanup();

    void PrepareFrameDataForRender(Renderer::FrameBuffer& frameBuffer);

    AssetLoad::RequestLoad RequestModelLoad(const char* path);

    AssetLoad::RuntimeMeshHandle GenerateModel(AssetLoad::ModelEntryHandle modelEntryHandle, const Transform& topLevelTransform);

    bool UpdateRuntimeMesh(AssetLoad::RuntimeMeshHandle runtimeMeshHandle, const Transform& topLevelTransform);

private:
    void UpdateTransforms(AssetLoad::RuntimeMesh* runtimeMesh);

private:
    static Engine* instance;

    SDL_Window* window{nullptr};

    Game::GameState gameState;
    Core::EngineSynchronization engineSynchronization{};

private:
    Render::RenderThread renderThread{};
    AssetLoad::AssetLoadingThread assetLoadingThread{};

    uint64_t gameFrame{0};
    uint64_t renderFrame{0};

    bool bShouldExit{false};
    bool bSwapchainOutdated{false};

    std::chrono::time_point<std::chrono::steady_clock> start{};

private: // Game -> Render command cache
    Renderer::RawSceneData rawSceneData;

    RingBuffer<AssetLoad::ModelEntryHandle, AssetLoad::MAX_LOADED_MODEL_RING_BUFFER> loadedModelEntryHandles;
    std::vector<VkBufferMemoryBarrier2> bufferAcquireOperations;
    std::vector<VkImageMemoryBarrier2> imageAcquireOperations;

    std::vector<Renderer::ModelMatrixOperation> modelMatrixOperations;
    std::vector<Renderer::InstanceOperation> instanceOperations;
    std::vector<Renderer::JointMatrixOperation> jointMatrixOperations;

    FreeList<AssetLoad::RuntimeMesh, AssetLoad::MAX_RUNTIME_MESH> runtimeMeshes;

private: // Game DLL Loading
    Utils::DllLoader gameDll;
    GameFunctions gameFunctions = {
        StubInit,
        StubUpdate,
        StubShutdown,
    };
};
} // HotReloading::Engine

#endif //WILLENGINETESTBED_ENGINE_H
