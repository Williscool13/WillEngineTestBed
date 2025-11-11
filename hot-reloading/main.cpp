#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <fmt/format.h>

#include "hot-reloading.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== Hot Reloading ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/hot-reloading.log");


    HotReloading::HotReloading hr{};
    hr.Initialize();
    hr.Run();
    hr.Cleanup();

    return 0;
}
