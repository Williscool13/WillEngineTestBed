add_library(hot-reload-engine STATIC
        engine.h
        engine.cpp
)

target_link_libraries(hot-reload-engine PUBLIC
        engine-common
        crash-handling
        core-lib
        render-lib
        input-lib
        SDL3::SDL3
)