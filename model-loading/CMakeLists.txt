add_executable(model-loading
        main.cpp
        model_loading.cpp
        model_loading.h
)

target_link_libraries(model-loading PRIVATE
        engine-common
        crash-handling
        core-lib
        render-lib
        input-lib
)

add_custom_command(TARGET model-loading POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:model-loading>
)

compile_shaders(model-loading
        ${CMAKE_SOURCE_DIR}/shaders/indirectDraw.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/indirectDraw.slang fragment fragmentMain
        ${CMAKE_SOURCE_DIR}/shaders/drawCull.slang compute computeMain
        ${CMAKE_SOURCE_DIR}/shaders/skeletalIndirectDraw.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/skeletalIndirectDraw.slang fragment fragmentMain
)
