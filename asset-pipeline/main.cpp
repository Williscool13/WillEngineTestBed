#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <fmt/format.h>

#include "asset-pipeline.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== Asset Pipeline ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/asset-pipeline.log");

    AssetPipeline::AssetPipeline ap{};
    ap.Initialize();
    ap.Run();
    ap.Cleanup();

    return 0;
}
