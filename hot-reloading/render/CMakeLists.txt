add_library(hot-reload-render STATIC
        render.h
        render.cpp
)

target_link_libraries(hot-reload-render PUBLIC
        hot-reload-core
        engine-common
        crash-handling
        render-lib
        SDL3::SDL3
)