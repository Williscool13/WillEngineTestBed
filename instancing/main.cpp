#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <fmt/format.h>

#include "instanced-rendering.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== Meshlet Rendering ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/meshlet-rendering.log");

    InstancedRendering::InstancedRendering mr{};
    mr.Initialize();
    mr.Run();
    mr.Cleanup();

    return 0;
}
