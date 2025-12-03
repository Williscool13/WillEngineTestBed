add_executable(ui-rendering
        main.cpp
        ui-rendering.cpp
        ui-rendering.h
)

add_custom_command(TARGET ui-rendering POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:ui-rendering>
)

target_link_libraries(ui-rendering PRIVATE
        engine-common
        crash-handling
        core-lib
        game-lib
        render-lib
        input-lib
        freetype
)

compile_shaders(ui-rendering
        ${CMAKE_SOURCE_DIR}/shaders/render2.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/render2.slang fragment fragmentMain
        ${CMAKE_SOURCE_DIR}/shaders/render3.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/render3.slang fragment fragmentMain
        ${CMAKE_SOURCE_DIR}/shaders/uiTextRendering.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/uiTextRendering.slang fragment fragmentMain
)