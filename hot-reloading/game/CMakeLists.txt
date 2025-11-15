add_library(hot-reload-game SHARED
        game.cpp
        game.h
        game_state.cpp
        game_state.h
        game_logging.cpp
        game_logging.h
        game_input.cpp
        game_input.h
        game_camera.cpp
        game_camera.h
)

target_link_libraries(hot-reload-game PUBLIC
        hot-reload-engine
        hot-reload-asset-load
        engine-common
        core-lib
        input-lib
        SDL3::SDL3
)

target_compile_definitions(hot-reload-game PRIVATE GAME_EXPORTS)

