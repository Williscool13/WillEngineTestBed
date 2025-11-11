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

add_subdirectory(game)

set(DLLS_TO_COPY
        $<TARGET_FILE:hot-reload-game>
        $<TARGET_FILE:SDL3::SDL3>
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(PDBS_TO_COPY
            $<TARGET_PDB_FILE:hot-reload-game>
            $<TARGET_PDB_FILE:SDL3::SDL3>
    )
endif()

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dlls_copied.stamp
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DLLS_TO_COPY} $<TARGET_FILE_DIR:hot-reloading>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PDBS_TO_COPY} $<TARGET_FILE_DIR:hot-reloading>
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/dlls_copied.stamp
        DEPENDS hot-reload-game SDL3::SDL3
        COMMENT "Copying DLLs and PDBs"
)

add_custom_target(copy-runtime-deps ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dlls_copied.stamp
)

add_dependencies(hot-reloading copy-runtime-deps)
