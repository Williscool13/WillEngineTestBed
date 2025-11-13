add_executable(hot-reloading
        main.cpp
)

target_link_libraries(hot-reloading PRIVATE
        hot-reload-engine
        hot-reload-render
)

add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(render)

add_dependencies(hot-reloading hot-reload-game)

add_custom_command(TARGET hot-reloading POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:hot-reloading>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_PDB_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:hot-reloading>
)

compile_shaders(hot-reloading
        ${CMAKE_SOURCE_DIR}/shaders/indirectDraw.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/indirectDraw.slang fragment fragmentMain
        ${CMAKE_SOURCE_DIR}/shaders/drawCull.slang compute computeMain
        ${CMAKE_SOURCE_DIR}/shaders/skeletalIndirectDraw.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/skeletalIndirectDraw.slang fragment fragmentMain
)