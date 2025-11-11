add_executable(hot-reloading
        main.cpp
        hot-reloading.cpp
        hot-reloading.h
)

target_link_libraries(hot-reloading PRIVATE
        engine-common
        crash-handling
        render-lib
        input-lib
)

add_custom_command(TARGET hot-reloading POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:hot-reloading>
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_custom_command(TARGET hot-reloading POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_PDB_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:hot-reloading>
    )
endif()

add_subdirectory(game)

add_custom_command(TARGET hot-reloading POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:hot-reload-game>
        $<TARGET_FILE_DIR:hot-reloading>
        COMMENT "Copying hot-reload-game.dll to engine directory"
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_custom_command(TARGET hot-reloading POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_PDB_FILE:hot-reload-game>
            $<TARGET_FILE_DIR:hot-reloading>
            COMMENT "Copying hot-reload-game.pdb to engine directory"
    )
endif()
