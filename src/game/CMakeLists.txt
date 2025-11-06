add_library(game-lib STATIC
        camera/free_camera.cpp
        camera/free_camera.h
        camera/camera.cpp
        camera/camera.h
)

target_include_directories(game-lib INTERFACE
        ${CMAKE_SOURCE_DIR}/extern/SDL/include
)

target_link_libraries(game-lib PUBLIC
        engine-common
        SDL3::SDL3
        input-lib
)