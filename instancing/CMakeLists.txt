add_executable(instanced-rendering
        main.cpp
        instanced-rendering.cpp
        instanced-rendering.h
)


target_link_libraries(instanced-rendering PRIVATE
        engine-common
        crash-handling
        core-lib
        game-lib
        render-lib
        input-lib
        meshoptimizer
)

add_custom_command(TARGET instanced-rendering POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:instanced-rendering>
)

compile_shader(instanced-rendering ${CMAKE_SOURCE_DIR}/shaders/gpuInstancing.slang compute computeVisibility OUTPUT_NAME instancingVisibility)
compile_shader(instanced-rendering ${CMAKE_SOURCE_DIR}/shaders/gpuInstancing.slang compute computePrefixSum OUTPUT_NAME instancingPrefixSum)
compile_shader(instanced-rendering ${CMAKE_SOURCE_DIR}/shaders/gpuInstancing.slang compute computeCompactAndGenerateIndirect OUTPUT_NAME instancingCompactAndGenerateIndirect)
compile_shaders(instanced-rendering
        ${CMAKE_SOURCE_DIR}/shaders/meshShaderInstancing.slang task taskMain
        ${CMAKE_SOURCE_DIR}/shaders/meshShaderInstancing.slang mesh meshMain
        ${CMAKE_SOURCE_DIR}/shaders/meshShaderInstancing.slang fragment fragmentMain)
