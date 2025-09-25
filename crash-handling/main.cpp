#include <iostream>
#include <fmt/format.h>

int main()
{
    fmt::println("Crash Handling Started");

    for (int i = 1; i <= 5; i++) {
        fmt::println("i = {}", i);
    }

    fmt::println("Test");
    return 0;
}
