#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "skeletal_main.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/skeletal-main.log");

    Renderer::SkeletalMain s{};
    s.Initialize();
    s.Run();
    s.Cleanup();
    return 0;
}
