//
// Created by William on 2025-10-07.
//

#ifndef WILLENGINETESTBED_MULTITHREADING_H
#define WILLENGINETESTBED_MULTITHREADING_H
#include "types.h"


class Multithreading
{
public:
    Multithreading() = default;

    void Initialize();

    void Run();

    void Cleanup();

private:
    struct SDL_Window* window{nullptr};

    FrameData data[2]{};
};


#endif //WILLENGINETESTBED_MULTITHREADING_H