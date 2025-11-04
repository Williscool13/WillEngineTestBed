//
// Created by William on 2025-10-31.
//

#include "engine_multithreading.h"

#include "core/constants.h"
#include "core/time.h"
#include "crash-handling/crash_handler.h"
#include "input/input.h"
#include "render/vk_imgui_wrapper.h"
#include "utils/utils.h"

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

        if (input.IsKeyPressed(Key::G)) {
            auto suzannePath = std::filesystem::path("../assets/Suzanne/glTF/Suzanne.gltf");
            assetLoadingThread.RequestLoad(suzannePath, [this](Renderer::ModelEntryHandle handle) {
                if (handle == Renderer::ModelEntryHandle::Invalid) {
                    LOG_ERROR("Failed to load Suzanne model");
                    return;
                }

                // Model is loaded and ready to use
                // Store handle for rendering
                // loadedModels.push_back(handle);

                // Or immediately spawn an entity with it
                // SpawnEntity(handle);
            });
        }
        // game logicz
        // bla bla bla bla

        gameFrames.acquire();
        //
        {
            uint32_t currentFrameInFlight = frameNumber % Core::FRAMES_IN_FLIGHT;

            frameBuffers[currentFrameInFlight].currentFrame = frameNumber;
            //LOG_INFO("[Game Thread] Processed frame {}", frameNumber);
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
