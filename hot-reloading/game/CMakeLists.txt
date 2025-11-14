add_library(hot-reload-game SHARED
        game.cpp
        game.h
        game_state.cpp
        game_state.h
        game_logging.h
        game_logging.h
)

target_link_libraries(hot-reload-game PUBLIC
        hot-reload-engine
        engine-common
        crash-handling
        core-lib
        input-lib
        SDL3::SDL3
)

target_compile_definitions(hot-reload-game PRIVATE GAME_EXPORTS)

