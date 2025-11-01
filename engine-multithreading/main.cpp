#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <fmt/format.h>

#include "engine_multithreading.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== Engine Multithreading ===");


    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/engine_multithreading.log");

    EngineMultithreading em{};
    em.Initialize();
    em.Run();
    em.Cleanup();

    return 0;
}