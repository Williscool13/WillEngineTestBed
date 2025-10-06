#include <iostream>
#include <fmt/format.h>

#include "src/crash-handling/crash_context.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"

int main()
{
    fmt::println("=== Crash Handler Test ===");

    CrashHandler::Initialize("crashes/");

    Logger::Initialize("logs/engine.log");

    CrashContext::Initialize();


    fmt::println("\nSelect crash type:");
    fmt::println("1. Null pointer dereference");
    fmt::println("2. Out of bounds access");
    fmt::println("3. Division by zero");
    fmt::println("4. Manual dump (no crash)");
    fmt::println("5. Exit normally");

    int32_t choice;
    std::cout << "Enter choice: ";
    std::cin >> choice;

    LOG_INFO("User selected option: {}", choice);

    switch (choice) {
        case 1:
        {
            LOG_ERROR("About to dereference null pointer - this will crash!");
            int* nullPtr = nullptr;
            *nullPtr = 42;
            break;
        }
        case 2:
        {
            LOG_ERROR("About to access out of bounds - this will crash!");
            const std::vector vec = {1, 2, 3};
            int32_t bad = vec[100];
            break;
        };
        case 3:
        {
            LOG_ERROR("About to divide by zero - this will crash!");

            int32_t a = 10;
            int32_t b = 0;
            volatile int32_t result = a / b;
            break;
        }
        case 4:
            LOG_INFO("Creating manual dump as requested by user");
            CrashHandler::TriggerManualDump("User requested test dump");
            LOG_INFO("Manual dump created - program continues normally");
            break;
        case 5:
            LOG_INFO("Exiting normally");
            break;
        default:
            LOG_WARN("Invalid choice: {}", choice);
            break;
    }

    return 0;
}
