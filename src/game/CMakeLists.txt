add_library(game-lib STATIC
        camera/free_camera.cpp
        camera/free_camera.h
        camera/camera.cpp
        camera/camera.h
)

target_link_libraries(game-lib PUBLIC
        engine-common
        input-lib
)