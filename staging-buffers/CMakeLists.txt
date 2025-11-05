add_executable(staging-buffer
        main.cpp
        staging_buffer.cpp
        staging_buffer.h
        staging_uploader.cpp
        staging_uploader.h
)

add_custom_command(TARGET staging-buffer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:staging-buffer>
)

target_link_libraries(staging-buffer PRIVATE
        engine-common
        crash-handling
        core-lib
        render-lib
        input-lib
)


compile_shaders(staging-buffer
        ${CMAKE_SOURCE_DIR}/shaders/indirectDraw.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/indirectDraw.slang fragment fragmentMain
        ${CMAKE_SOURCE_DIR}/shaders/drawCull.slang compute computeMain
        ${CMAKE_SOURCE_DIR}/shaders/skeletalIndirectDraw.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/skeletalIndirectDraw.slang fragment fragmentMain
)