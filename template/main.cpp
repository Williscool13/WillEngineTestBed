#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <fmt/format.h>

#include "class-name.h"
#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== Template ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/template.log");

    Template::ClassName mb{};
    mb.Initialize();
    mb.Run();
    mb.Cleanup();

    return 0;
}
