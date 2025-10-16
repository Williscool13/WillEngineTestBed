#define SDL_MAIN_HANDLED

#include "audio.h"
#include "crash-handling/crash_context.h"
#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"

int main()
{
    fmt::println("=== Audio (SDL3) ===");

    CrashHandler::Initialize("crashes/");
    CrashContext::Initialize();
    Logger::Initialize("logs/audio.log");

    Audio::Audio a{};
    a.Init();
    a.Update();
    a.Cleanup();


    return 0;
}
