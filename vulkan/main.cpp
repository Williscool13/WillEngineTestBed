#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <fmt/format.h>

#include "renderer.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== Vulkan Rendering ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/vulkan-renderer.log");

    Renderer::Renderer r{};
    r.Initialize();
    r.Run();
    r.Cleanup();

    return 0;
}
