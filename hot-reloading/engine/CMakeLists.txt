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

add_custom_command(TARGET hot-reload-engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:hot-reload-engine>
        $<TARGET_FILE_DIR:hot-reloading>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_PDB_FILE:hot-reload-engine>
        $<TARGET_FILE_DIR:hot-reloading>
)