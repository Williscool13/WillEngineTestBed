#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <fmt/format.h>

#include "ui-rendering.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== UI Rendering ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/ui-rendering.log");

    UIRendering::UIRendering mb{};
    mb.Initialize();
    mb.Run();
    mb.Cleanup();

    return 0;
}
