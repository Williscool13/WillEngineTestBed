add_executable(ktx-export
        main.cpp
        ktx-export.cpp
        ktx-export.h
        model_serialization.cpp
        model_serialization.h
)


target_link_libraries(ktx-export PRIVATE
        engine-common
        crash-handling
        core-lib
        game-lib
        render-lib
        input-lib
        meshoptimizer
)

add_custom_command(TARGET ktx-export POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:ktx-export>
)

compile_shader(ktx-export ${CMAKE_SOURCE_DIR}/shaders/gpuInstancing.slang compute computeVisibility OUTPUT_NAME instancingVisibility)
compile_shader(ktx-export ${CMAKE_SOURCE_DIR}/shaders/gpuInstancing.slang compute computePrefixSum OUTPUT_NAME instancingPrefixSum)
compile_shader(ktx-export ${CMAKE_SOURCE_DIR}/shaders/gpuInstancing.slang compute computeCompactAndGenerateIndirect OUTPUT_NAME instancingCompactAndGenerateIndirect)
compile_shaders(ktx-export
        ${CMAKE_SOURCE_DIR}/shaders/meshShaderInstancing.slang task taskMain
        ${CMAKE_SOURCE_DIR}/shaders/meshShaderInstancing.slang mesh meshMain
        ${CMAKE_SOURCE_DIR}/shaders/meshShaderInstancing.slang fragment fragmentMain)
