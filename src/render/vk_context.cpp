//
// Created by William on 2025-10-09.
//

#include "vk_context.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vk-bootstrap/src/VkBootstrap.h>
#include <vulkan/vk_enum_string_helper.h>

#include "crash-handling/crash_handler.h"
#include "crash-handling/logger_helpers.h"

namespace Renderer
{
DeviceInfo VulkanContext::deviceInfo{};

VulkanContext::VulkanContext(SDL_Window* window)
{
    VkResult res = volkInitialize();
    if (res != VK_SUCCESS) {
        LOG_ERROR("Failed to initialize volk: {}", string_VkResult(res));
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    vkb::InstanceBuilder builder;
    std::vector<const char*> enabledInstanceExtensions;

#ifdef NDEBUG
    bool bUseValidation = false;
#else
    bool bUseValidation = true;
    enabledInstanceExtensions.push_back("VK_EXT_debug_utils");
#endif


    auto resultInstance = builder.set_app_name("Vulkan Test Bed")
            .request_validation_layers(bUseValidation)
            .use_default_debug_messenger()
            .require_api_version(1, 3)
            .enable_extensions(enabledInstanceExtensions)
            .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT)
            .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
            .build();

    vkb::Instance vkb_inst = resultInstance.value();
    instance = vkb_inst.instance;
    volkLoadInstanceOnly(instance);
    debugMessenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);

    VkPhysicalDeviceVulkan13Features features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    VkPhysicalDeviceVulkan11Features features11{};
    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    VkPhysicalDeviceFeatures otherFeatures{};

    // Descriptor Buffer Extension
    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures = {};
    descriptorBufferFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    descriptorBufferFeatures.descriptorBuffer = VK_TRUE;

    // Task/Mesh Shader Extension
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = {};
    meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
    meshShaderFeatures.taskShader = VK_TRUE;
    meshShaderFeatures.meshShader = VK_TRUE;

    // Modern Rendering (Vulkan 1.3)
    features.dynamicRendering = VK_TRUE;
    features.synchronization2 = VK_TRUE;

    // GPU Driven Rendering
    features12.bufferDeviceAddress = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    features12.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
    features12.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
    features12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    features12.drawIndirectCount = VK_TRUE;
    otherFeatures.multiDrawIndirect = VK_TRUE;

    // SV_VertexID
    features11.shaderDrawParameters = VK_TRUE;

    // uint8_t/int8_t support
    features12.shaderInt8 = VK_TRUE;

    // uint64_t/uint64 support
    otherFeatures.shaderInt64 = VK_TRUE;


    vkb::PhysicalDeviceSelector selector{vkb_inst};
    vkb::PhysicalDevice targetDevice = selector
            .set_minimum_version(1, 3)
            .set_required_features_13(features)
            .set_required_features_12(features12)
            .set_required_features_11(features11)
            .set_required_features(otherFeatures)
            .add_required_extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME)
            .add_required_extension(VK_EXT_MESH_SHADER_EXTENSION_NAME)
            .set_surface(surface)
            .select()
            .value();

    vkb::DeviceBuilder deviceBuilder{targetDevice};
    deviceBuilder.add_pNext(&descriptorBufferFeatures);
    deviceBuilder.add_pNext(&meshShaderFeatures);
    vkb::Device vkbDevice = deviceBuilder.build().value();

    device = vkbDevice.device;
    volkLoadDevice(device);
    physicalDevice = targetDevice.physical_device;

    // Queues and queue family
    {
        auto graphicsQueueResult = vkbDevice.get_queue(vkb::QueueType::graphics);
        if (!graphicsQueueResult) {
            LOG_ERROR("Failed to get graphics queue: {}", graphicsQueueResult.error().message());
            CrashHandler::TriggerManualDump();
            exit(1);
        }
        graphicsQueue = graphicsQueueResult.value();

        auto graphicsQueueFamilyResult = vkbDevice.get_queue_index(vkb::QueueType::graphics);
        if (!graphicsQueueFamilyResult) {
            LOG_ERROR("Failed to get graphics queue family index: {}", graphicsQueueFamilyResult.error().message());
            CrashHandler::TriggerManualDump();
            exit(1);
        }
        graphicsQueueFamily = graphicsQueueFamilyResult.value();

        auto transferQueueResult = vkbDevice.get_queue(vkb::QueueType::transfer);
        if (!transferQueueResult) {
            LOG_ERROR("Failed to get transfer queue: {}", transferQueueResult.error().message());
            CrashHandler::TriggerManualDump();
            exit(1);
        }
        transferQueue = transferQueueResult.value();

        auto transferQueueFamilyResult = vkbDevice.get_queue_index(vkb::QueueType::transfer);
        if (!transferQueueFamilyResult) {
            LOG_ERROR("Failed to get transfer queue family index: {}", transferQueueFamilyResult.error().message());
            CrashHandler::TriggerManualDump();
            exit(1);
        }
        transferQueueFamily = transferQueueFamilyResult.value();
    }

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    vmaCreateAllocator(&allocatorInfo, &allocator);


    LOG_INFO("=== Vulkan Context Handles ===");
    LOG_INFO("Instance:              0x{:016x}", reinterpret_cast<uintptr_t>(instance));
    LOG_INFO("Surface:               0x{:016x}", reinterpret_cast<uintptr_t>(surface));
    LOG_INFO("Physical Device:       0x{:016x}", reinterpret_cast<uintptr_t>(physicalDevice));
    LOG_INFO("Device:                0x{:016x}", reinterpret_cast<uintptr_t>(device));
    LOG_INFO("Graphics Queue:        0x{:016x}", reinterpret_cast<uintptr_t>(graphicsQueue));
    LOG_INFO("Graphics Queue Family: {}", graphicsQueueFamily);
    LOG_INFO("Transfer Queue:        0x{:016x}", reinterpret_cast<uintptr_t>(transferQueue));
    LOG_INFO("Transfer Queue Family: {}", transferQueueFamily);
    LOG_INFO("VMA Allocator:         0x{:016x}", reinterpret_cast<uintptr_t>(allocator));

    deviceInfo.properties.pNext = &deviceInfo.descriptorBufferProps;
    deviceInfo.descriptorBufferProps.pNext = &deviceInfo.meshShaderProps;
    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceInfo.properties);
}

VulkanContext::~VulkanContext()
{
    vmaDestroyAllocator(allocator);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);
    vkb::destroy_debug_utils_messenger(instance, debugMessenger);
    vkDestroyInstance(instance, nullptr);
}
} // Renderer
