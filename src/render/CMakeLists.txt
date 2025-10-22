add_library(render-lib STATIC
        vk_context.cpp
        vk_context.h
        vk_swapchain.cpp
        vk_swapchain.h
        vk_synchronization.cpp
        vk_synchronization.h
        vk_imgui_wrapper.cpp
        vk_imgui_wrapper.h
        vk_descriptors.h
        vk_descriptors.cpp
        vk_pipelines.cpp
        vk_pipelines.h
        vk_resources.cpp
        vk_resources.h
        vk_helpers.cpp
        vk_helpers.h
        vk_types.cpp
        vk_types.h
        render_utils.cpp
        render_utils.h
        render_constants.cpp
        render_constants.h
        render_context.cpp
        render_context.h
        render_targets.cpp
        render_targets.h
        model/model_loader.cpp
        model/model_loader.h
        model/model_data.cpp
        model/model_data.h
        descriptor_buffer/descriptor_buffer_uniform.cpp
        descriptor_buffer/descriptor_buffer_uniform.h
        descriptor_buffer/descriptor_buffer_storage.cpp
        descriptor_buffer/descriptor_buffer_storage.h
        descriptor_buffer/descriptor_buffer_combined_image_sampler.cpp
        descriptor_buffer/descriptor_buffer_combined_image_sampler.h
        descriptor_buffer/descriptor_buffer_sampled_image.cpp
        descriptor_buffer/descriptor_buffer_sampled_image.h
        descriptor_buffer/descriptor_buffer_sampler.cpp
        descriptor_buffer/descriptor_buffer_sampler.h
        descriptor_buffer/descriptor_buffer_storage_image.cpp
        descriptor_buffer/descriptor_buffer_storage_image.h
        descriptor_buffer/descripor_buffer_bindless_resources.cpp
        descriptor_buffer/descripor_buffer_bindless_resources.h
)

target_link_libraries(render-lib PUBLIC
        engine-common
        crash-handling
        volk
        imgui
        VulkanMemoryAllocator
        vk-bootstrap::vk-bootstrap
        SDL3::SDL3
        fastgltf
        offsetAllocator
)

find_program(SLANGC slangc REQUIRED)

set(COMMON_SHADER_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/common.slang
)

function(compile_shader TARGET SHADER_SOURCE STAGE ENTRY)
    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME_WE)
    set(SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}_${STAGE}.spv)

    add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
            COMMAND ${SLANGC} ${SHADER_SOURCE} -target spirv -entry ${ENTRY} -stage ${STAGE} -o ${SHADER_OUTPUT}
            DEPENDS ${SHADER_SOURCE} ${COMMON_SHADER_INCLUDES}
            COMMENT "Compiling ${STAGE} shader from ${SHADER_SOURCE}"
    )

    target_sources(${TARGET} PRIVATE ${SHADER_OUTPUT})
endfunction()


function(compile_shaders TARGET)
    set(ARGS ${ARGN})
    list(LENGTH ARGS NUM_ARGS)

    math(EXPR NUM_TRIPLETS "${NUM_ARGS} / 3")

    foreach (i RANGE 0 ${NUM_TRIPLETS})
        math(EXPR SOURCE_IDX "${i} * 3")
        math(EXPR STAGE_IDX "${i} * 3 + 1")
        math(EXPR ENTRY_IDX "${i} * 3 + 2")

        if (SOURCE_IDX LESS NUM_ARGS)
            list(GET ARGS ${SOURCE_IDX} SHADER_SOURCE)
            list(GET ARGS ${STAGE_IDX} STAGE)
            list(GET ARGS ${ENTRY_IDX} ENTRY)
            compile_shader(${TARGET} ${SHADER_SOURCE} ${STAGE} ${ENTRY})
        endif ()
    endforeach ()
endfunction()