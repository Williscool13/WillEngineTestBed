
add_executable(engine-multithreading
        main.cpp
        engine_multithreading.cpp
        engine_multithreading.h
        render_thread.cpp
        render_thread.h
        asset_loading_thread.cpp
        asset_loading_thread.h
        asset_load_types.cpp
        asset_load_types.h
)

target_link_libraries(engine-multithreading PRIVATE
        engine-common
        crash-handling
        core-lib
        game-lib
        render-lib
        input-lib
)

add_custom_command(TARGET engine-multithreading POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:engine-multithreading>
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_custom_command(TARGET engine-multithreading POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_PDB_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:engine-multithreading>
    )
endif()

compile_shaders(engine-multithreading
        ${CMAKE_SOURCE_DIR}/shaders/indirectDraw.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/indirectDraw.slang fragment fragmentMain
        ${CMAKE_SOURCE_DIR}/shaders/drawCull.slang compute computeMain
        ${CMAKE_SOURCE_DIR}/shaders/skeletalIndirectDraw.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/skeletalIndirectDraw.slang fragment fragmentMain
)

compile_shaders(engine-multithreading
        ${CMAKE_SOURCE_DIR}/shaders/compute.slang compute computeMain
)
