add_executable(renderer
        main.cpp
        renderer.cpp
        renderer.h
        vulkan_context.cpp
        vulkan_context.h
        imgui_wrapper.cpp
        imgui_wrapper.h
        utils.cpp
        utils.h
        swapchain.cpp
        swapchain.h
        synchronization.cpp
        synchronization.h
        input.cpp
        input.h
        vk_helpers.cpp
        vk_helpers.h
        vk_resources.cpp
        vk_resources.h
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
        vk_descriptors.h
        vk_descriptors.cpp

)

add_custom_command(TARGET renderer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:renderer>
)

target_link_libraries(renderer PRIVATE
        engine-common
        crash-handling
        volk
        imgui
        VulkanMemoryAllocator
        vk-bootstrap::vk-bootstrap
        SDL3::SDL3
)