add_executable(hot-reloading
        main.cpp
)

target_link_libraries(hot-reloading PRIVATE
        hot-reload-engine
)

add_subdirectory(engine)
add_subdirectory(game)

add_dependencies(hot-reloading hot-reload-game)

add_custom_command(TARGET hot-reloading POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:hot-reloading>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_PDB_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:hot-reloading>
)
