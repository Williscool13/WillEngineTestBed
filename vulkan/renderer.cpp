//
// Created by William on 2025-10-09.
//

#include "renderer.h"

#include <volk.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <VkBootstrap.h>
#include <SDL3/SDL.h>

#include <vulkan/vk_enum_string_helper.h>

#include "SDL3/SDL_vulkan.h"
#include "src/crash-handling/crash_handler.h"
#include "src/crash-handling/logger.h"


namespace Renderer
{
DeviceInfo Renderer::deviceInfo = {};

void Renderer::Initialize()
{
    bool sdlInitSuccess = SDL_Init(SDL_INIT_VIDEO);
    if (!sdlInitSuccess) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    constexpr auto window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    window = SDL_CreateWindow(
        "Vulkan Test Bed",
        800,
        600,
        window_flags);

    if (VkResult res = volkInitialize(); res != VK_SUCCESS) {
        LOG_ERROR("Failed to initialize volk: {}", string_VkResult(res));
        CrashHandler::TriggerManualDump();
        exit(1);
    }

    vkb::InstanceBuilder builder;
    std::vector<const char*> enabledInstanceExtensions;
    enabledInstanceExtensions.push_back("VK_EXT_debug_utils");

#ifdef NDEBUG
    bool bUseValidation = false;
#else
    bool bUseValidation = true;
#endif


    auto resultInstance = builder.set_app_name("Vulkan Test Bed")
            .request_validation_layers(bUseValidation)
            .use_default_debug_messenger()
            .require_api_version(1, 4)
            .enable_extensions(enabledInstanceExtensions)
            .build();

    vkb::Instance vkb_inst = resultInstance.value();
    instance = vkb_inst.instance;
    volkLoadInstance(instance);
    debugMessenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);

    // vk 1.3
    VkPhysicalDeviceVulkan13Features features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features.dynamicRendering = VK_TRUE;
    features.synchronization2 = VK_TRUE;

    // vk 1.2
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    // Descriptor Buffer Extension
    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures = {};
    descriptorBufferFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    descriptorBufferFeatures.descriptorBuffer = VK_TRUE;

    vkb::PhysicalDeviceSelector selector{vkb_inst};
    vkb::PhysicalDevice targetDevice = selector
            .set_minimum_version(1, 4)
            .set_required_features_13(features)
            .set_required_features_12(features12)
            .add_required_extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME)
            .set_surface(surface)
            .select()
            .value();

    vkb::DeviceBuilder deviceBuilder{targetDevice};
    deviceBuilder.add_pNext(&descriptorBufferFeatures);
    vkb::Device vkbDevice = deviceBuilder.build().value();

    device = vkbDevice.device;
    volkLoadDevice(device);
    physicalDevice = targetDevice.physical_device;

    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

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


    VkPhysicalDeviceProperties2 deviceProperties{};
    deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties.pNext = &deviceInfo.descriptorBufferProps;
    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties);
    deviceInfo.properties = deviceProperties.properties;
}

void Renderer::Run()
{
    SDL_Event e;
    bool exit = false;
    while (true) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_EVENT_QUIT) { exit = true; }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) { exit = true; }
        }

        if (exit) {
            bShouldExit = true;
            break;
        }

        auto wait = std::chrono::milliseconds(100);
        std::this_thread::sleep_for(wait);
    }
}

void Renderer::Cleanup()
{
    vmaDestroyAllocator(allocator);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);
    vkb::destroy_debug_utils_messenger(instance, debugMessenger);
    vkDestroyInstance(instance, nullptr);

    SDL_DestroyWindow(window);
}
}
