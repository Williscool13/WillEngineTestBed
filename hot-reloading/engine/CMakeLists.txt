add_library(hot-reload-engine SHARED
        engine.h
        engine.cpp
        engine_api.cpp
        engine_api.h
)

target_link_libraries(hot-reload-engine PUBLIC
        hot-reload-render
        engine-common
        crash-handling
        core-lib
        render-lib
        input-lib
        SDL3::SDL3
)

target_compile_definitions(hot-reload-engine PRIVATE ENGINE_EXPORTS)