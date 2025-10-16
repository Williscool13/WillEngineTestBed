add_library(input-lib STATIC
        input.cpp
        input.h
)

target_link_libraries(input-lib PUBLIC
        engine-common
        SDL3::SDL3
)