#include <iostream>
#include <fmt/format.h>

#include "crash_context.h"
#include "crash_handler.h"

void StackOverflow()
{
    fmt::println("Recursing infinitely...");
    StackOverflow();
}

int main()
{
    fmt::println("=== Crash Handler Test ===");

    CrashContext::Initialize();
    CrashHandler::Initialize("crashes/");

    fmt::println("\nSelect crash type:");
    fmt::println("1. Null pointer dereference");
    fmt::println("2. Stack overflow");
    fmt::println("3. Division by zero");
    fmt::println("4. Manual dump (no crash)");
    fmt::println("5. Exit normally");

    int choice;
    std::cout << "Enter choice: ";
    std::cin >> choice;

    switch (choice) {
        case 1:
        {
            fmt::println("About to dereference null pointer...");
            int* nullPtr = nullptr;
            *nullPtr = 42;
            break;
        }
        case 2:
            StackOverflow();
            break;
        case 3:
        {
            fmt::println("About to divide by zero...");

            int32_t a = 10;
            int32_t b = 0;
            volatile int result = a / b;
            break;
        }
        case 4:
            CrashHandler::TriggerManualDump("User requested test dump");
            fmt::println("Manual dump created - program continues normally");
            break;
        case 5:
            fmt::println("Exiting normally");
            break;
        default:
            fmt::println("Invalid choice");
            break;
    }

    return 0;
}
