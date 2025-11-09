#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <fmt/format.h>

#include "single-buffering.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== Multi-Buffering ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/multi-buffering.log");

    Renderer::SingleBuffering mb{};
    mb.Initialize();
    mb.Run();
    mb.Cleanup();

    return 0;
}
