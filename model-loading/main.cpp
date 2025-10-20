#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "model_loading.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== Model Loading ===");


    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/cpu_profiling.log");


    Renderer::ModelLoading r{};
    r.Initialize();
    r.Run();
    r.Cleanup();

    return 0;
}