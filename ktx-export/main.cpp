#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <fmt/format.h>

#include "ktx-export.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== Ktx Export ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/ktx-export.log");

    KtxExport::KtxExport ke{};
    ke.Initialize();
    ke.Run();
    ke.Cleanup();

    return 0;
}
