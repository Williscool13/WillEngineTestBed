add_executable(physics
        main.cpp
        physics.cpp
        physics.h
)

target_link_libraries(physics PRIVATE
        engine-common
        crash-handling
        physics-lib
        Jolt
        enkiTS
)