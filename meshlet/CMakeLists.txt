add_executable(meshlet-rendering
        main.cpp
        meshlet-rendering.cpp
        meshlet-rendering.h
)


target_link_libraries(meshlet-rendering PRIVATE
        engine-common
        crash-handling
        core-lib
        game-lib
        render-lib
        input-lib
        meshoptimizer
)

add_custom_command(TARGET meshlet-rendering POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:meshlet-rendering>
)


compile_shaders(meshlet-rendering
        ${CMAKE_SOURCE_DIR}/shaders/render.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/render.slang fragment fragmentMain
        ${CMAKE_SOURCE_DIR}/shaders/render2.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/render2.slang fragment fragmentMain
        ${CMAKE_SOURCE_DIR}/shaders/basicMeshShader.slang task taskMain
        ${CMAKE_SOURCE_DIR}/shaders/basicMeshShader.slang mesh meshMain
        ${CMAKE_SOURCE_DIR}/shaders/basicMeshShader.slang fragment fragmentMain
        ${CMAKE_SOURCE_DIR}/shaders/meshShader.slang task taskMain
        ${CMAKE_SOURCE_DIR}/shaders/meshShader.slang mesh meshMain
        ${CMAKE_SOURCE_DIR}/shaders/meshShader.slang fragment fragmentMain
        ${CMAKE_SOURCE_DIR}/shaders/meshShaderIndirect.slang task taskMain
        ${CMAKE_SOURCE_DIR}/shaders/meshShaderIndirect.slang mesh meshMain
        ${CMAKE_SOURCE_DIR}/shaders/meshShaderIndirect.slang fragment fragmentMain
        ${CMAKE_SOURCE_DIR}/shaders/meshDrawCull.slang compute computeMain
)