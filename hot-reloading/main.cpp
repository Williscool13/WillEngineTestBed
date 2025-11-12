#include "engine/engine.h"


int main()
{
    using HotReloading::Engine::Engine;

    Engine engine{};
    engine.Initialize();
    engine.Run();
    engine.Cleanup();

    return 0;
}
