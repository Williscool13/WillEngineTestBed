add_executable(skeletal-mesh
        main.cpp
        skeletal_main.cpp
        skeletal_main.h
)

target_link_libraries(skeletal-mesh PRIVATE
        engine-common
        crash-handling
        render-lib
        input-lib
)

add_custom_command(TARGET skeletal-mesh POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:skeletal-mesh>
)

compile_shaders(skeletal-mesh
        ${CMAKE_SOURCE_DIR}/shaders/compute.slang compute computeMain
        ${CMAKE_SOURCE_DIR}/shaders/render.slang vertex vertexMain
        ${CMAKE_SOURCE_DIR}/shaders/render.slang fragment fragmentMain
)