add_executable(asset-pipeline
        main.cpp
        asset-pipeline.cpp
        asset-pipeline.h
        mesh_shader_types.cpp
        mesh_shader_types.h
)


target_link_libraries(asset-pipeline PRIVATE
        engine-common
        crash-handling
        render-lib
        input-lib
        meshoptimizer
)

add_custom_command(TARGET asset-pipeline POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:asset-pipeline>
)


compile_shaders(asset-pipeline
        #${CMAKE_SOURCE_DIR}/shaders/meshShader.slang mesh meshMain
        ${CMAKE_SOURCE_DIR}/shaders/render.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/render.slang fragment fragmentMain
)