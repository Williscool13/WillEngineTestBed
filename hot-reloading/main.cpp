#include "engine/engine.h"


int main()
{
    HotReloading::Engine::Engine engine{};
    engine.Initialize();
    engine.Run();
    engine.Cleanup();

    return 0;
}
