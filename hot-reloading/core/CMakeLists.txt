add_library(hot-reload-core STATIC
        engine_synchronization.cpp
        engine_synchronization.h
)

target_link_libraries(hot-reload-core PUBLIC
        core-lib
        render-lib
)