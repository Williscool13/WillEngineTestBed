add_library(core-lib STATIC
        types/transform.cpp
        types/transform.h
)

target_link_libraries(core-lib PUBLIC
        engine-common
)