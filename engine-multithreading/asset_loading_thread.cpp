//
// Created by William on 2025-10-20.
//

#include "asset_loading_thread.h"

#include "crash-handling/crash_handler.h"
#include "crash-handling/logger.h"
#include "render/render_utils.h"
#include "render/vk_helpers.h"
#include "stb/stb_image.h"

namespace Renderer
{
void AssetLoadingThread::Start()
{
    // start thread loop
}

void AssetLoadingThread::Stop()
{
    // .join the thread
}

void AssetLoadingThread::ThreadLoop()
{
    // check request queue
    // check if models are ready! If so, push to the `completeQueue`
}

void AssetLoadingThread::RequestLoad(const std::filesystem::path& path, const std::function<void(ModelEntryHandle)>& callback)
{
    requestQueue.push({path, callback});
}

void AssetLoadingThread::ResolveLoads()
{
    AssetLoadComplete loadComplete{};
    while (completeQueue.pop(loadComplete)) {
        loadComplete.onComplete(loadComplete.handle);
    }
}

ModelEntryHandle AssetLoadingThread::LoadGltf(const std::filesystem::path& path)
{
    if (auto it = pathToHandle.find(path); it != pathToHandle.end()) {
        models.Get(it->second)->refCount++;
        return it->second;
    }

    const ModelEntryHandle newModelHandle = models.Add();
    ModelEntry* newModel = models.Get(newModelHandle);
    newModel->refCount = 1;
    //newModel->data =;
}

void AssetLoadingThread::UnloadModel(ModelEntryHandle handle)
{
    // todo: Send unload command to render thread
    if (auto* entry = models.Get(handle)) {
        if (--entry->refCount == 0) {
            pathToHandle.erase(entry->data.path);
            models.Remove(handle);
        }
    }
}



void AssetLoadingThread::LoadGltfImages(const fastgltf::Asset& asset, const std::filesystem::path& parentFolder, std::vector<AllocatedImage>& outAllocatedImages)
{
    unsigned char* stbiData{nullptr};
    int32_t width{};
    int32_t height{};
    int32_t nrChannels{};


    TextureUploadStaging& textureStaging = GetAvailableTextureStaging();
    const VkCommandBufferBeginInfo cmdBeginInfo = VkHelpers::CommandBufferBeginInfo();
    VK_CHECK(vkBeginCommandBuffer(textureStaging.commandBuffer, &cmdBeginInfo));

    for (const fastgltf::Image& gltfImage : asset.images) {
        AllocatedImage newImage{};
        std::visit(
            fastgltf::visitor{
                [&](auto& arg) {},
                [&](const fastgltf::sources::URI& fileName) {
                    assert(fileName.fileByteOffset == 0); // We don't support offsets with stbi.
                    assert(fileName.uri.isLocalPath()); // We're only capable of loading
                    // local files.
                    const std::wstring widePath(fileName.uri.path().begin(), fileName.uri.path().end());
                    const std::filesystem::path fullPath = parentFolder / widePath;

                    // if (fullPath.extension() == ".ktx") {
                    //     ktxTexture1* kTexture;
                    //     const ktx_error_code_e ktxResult = ktxTexture1_CreateFromNamedFile(fullPath.string().c_str(),
                    //                                                                        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                    //                                                                        &kTexture);
                    //
                    //     if (ktxResult == KTX_SUCCESS) {
                    //         newImage = processKtxVector(resourceManager, kTexture);
                    //     }
                    //
                    //     ktxTexture1_Destroy(kTexture);
                    // }
                    // else if (fullPath.extension() == ".ktx2") {
                    //     ktxTexture2* kTexture;
                    //     const ktx_error_code_e ktxResult = ktxTexture2_CreateFromNamedFile(fullPath.string().c_str(),
                    //                                                                        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                    //                                                                        &kTexture);
                    //
                    //     if (ktxResult == KTX_SUCCESS) {
                    //         newImage = processKtxVector(resourceManager, kTexture);
                    //     }
                    //
                    //     ktxTexture2_Destroy(kTexture);
                    // }
                    // else {
                    stbiData = stbi_load(fullPath.string().c_str(), &width, &height, &nrChannels, 4);

                    // }
                },
                [&](const fastgltf::sources::Array& vector) {
                    bool bPass = true;
                    if (vector.bytes.size() > 30) {
                        // Minimum size for a meaningful check
                        std::string_view strData(reinterpret_cast<const char*>(vector.bytes.data()), std::min(size_t(100), vector.bytes.size()));

                        if (strData.find("https://git-lfs.github.com/spec") != std::string_view::npos) {
                            LOG_ERROR("Git LFS pointer detected instead of actual texture data for image: {}. ""Please run 'git lfs pull' to retrieve the actual files.", gltfImage.name.c_str());
                            bPass = false;
                        }
                    }

                    if (bPass) {
                        stbiData = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(vector.bytes.data()), static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);
                    }

                    //const int32_t ktxVersion = isKtxTexture(vector);
                    //switch (ktxVersion) {
                    // case 1:
                    // {
                    //     if (validateVector(vector, 0, vector.bytes.size())) {
                    //         ktxTexture1* kTexture;
                    //         const ktx_error_code_e ktxResult = ktxTexture1_CreateFromMemory(
                    //             reinterpret_cast<const unsigned char*>(vector.bytes.data()),
                    //             vector.bytes.size(),
                    //             KTX_TEXTURE_CREATE_NO_FLAGS,
                    //             &kTexture);
                    //
                    //
                    //         if (ktxResult == KTX_SUCCESS) {
                    //             newImage = processKtxVector(resourceManager, kTexture);
                    //         }
                    //
                    //         ktxTexture1_Destroy(kTexture);
                    //     }
                    //     else {
                    //         fmt::print("Error: Failed to validate vector data that contains ktx data\n");
                    //     }
                    // }
                    // break;
                    // case 2:
                    // {
                    //     if (validateVector(vector, 0, vector.bytes.size())) {
                    //         ktxTexture2* kTexture;
                    //
                    //         const KTX_error_code ktxResult = ktxTexture2_CreateFromMemory(
                    //             reinterpret_cast<const unsigned char*>(vector.bytes.data()),
                    //             vector.bytes.size(),
                    //             KTX_TEXTURE_CREATE_NO_FLAGS,
                    //             &kTexture);
                    //
                    //         if (ktxResult == KTX_SUCCESS) {
                    //             newImage = processKtxVector(resourceManager, kTexture);
                    //         }
                    //
                    //         ktxTexture2_Destroy(kTexture);
                    //     }
                    // }
                    // break;
                    // default:
                    // {
                    // }
                    // break;
                    //}
                },
                [&](const fastgltf::sources::BufferView& view) {
                    const fastgltf::BufferView& bufferView = asset.bufferViews[view.bufferViewIndex];
                    const fastgltf::Buffer& buffer = asset.buffers[bufferView.bufferIndex];
                    // We only care about VectorWithMime here, because we
                    // specify LoadExternalBuffers, meaning all buffers
                    // are already loaded into a vector.
                    std::visit(fastgltf::visitor{
                                   [](auto&) {},
                                   [&](const fastgltf::sources::Array& vector) {
                                       // const int32_t ktxVersion = isKtxTexture(vector);
                                       // switch (ktxVersion) {
                                       //     case 1:
                                       //     {
                                       //         if (validateVector(vector, 0, vector.bytes.size())) {
                                       //             ktxTexture1* kTexture;
                                       //             const KTX_error_code ktxResult = ktxTexture1_CreateFromMemory(
                                       //                 reinterpret_cast<const unsigned char*>(vector.bytes.data() + bufferView.byteOffset),
                                       //                 bufferView.byteLength,
                                       //                 KTX_TEXTURE_CREATE_NO_FLAGS,
                                       //                 &kTexture);
                                       //
                                       //
                                       //             if (ktxResult == KTX_SUCCESS) {
                                       //                 newImage = processKtxVector(resourceManager, kTexture);
                                       //             }
                                       //
                                       //             ktxTexture1_Destroy(kTexture);
                                       //         }
                                       //         else {
                                       //             fmt::print("Error: Failed to validate vector data that contains ktx data\n");
                                       //         }
                                       //     }
                                       //     break;
                                       //     case 2:
                                       //     {
                                       //         if (validateVector(vector, 0, vector.bytes.size())) {
                                       //             ktxTexture2* kTexture;
                                       //
                                       //             const KTX_error_code ktxResult = ktxTexture2_CreateFromMemory(
                                       //                 reinterpret_cast<const unsigned char*>(vector.bytes.data() + bufferView.byteOffset),
                                       //                 bufferView.byteLength,
                                       //                 KTX_TEXTURE_CREATE_NO_FLAGS,
                                       //                 &kTexture);
                                       //
                                       //             if (ktxResult == KTX_SUCCESS) {
                                       //                 newImage = processKtxVector(resourceManager, kTexture);
                                       //             }
                                       //
                                       //             ktxTexture2_Destroy(kTexture);
                                       //         }
                                       //         else {
                                       //             fmt::print("Error: Failed to validate vector data that contains ktx data\n");
                                       //         }
                                       //     }
                                       //     break;
                                       //     default:
                                       stbiData = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(vector.bytes.data() + bufferView.byteOffset), static_cast<int>(bufferView.byteLength), &width,
                                                                        &height,
                                                                        &nrChannels, 4);
                                       //
                                       //         break;
                                       // }
                                   }
                               }, buffer.data);
                }
            }, gltfImage.data);


        if (stbiData) {
            VkExtent3D imagesize;
            imagesize.width = width;
            imagesize.height = height;
            imagesize.depth = 1;
            const size_t size = width * height * 4;

            OffsetAllocator::Allocation allocation = textureStaging.stagingAllocator.allocate(size);
            if (allocation.metadata == OffsetAllocator::Allocation::NO_SPACE) {
                LOG_ERROR("Texture too large to fit in staging buffer. Increase staging buffer size or do not load this texture.");
                CrashHandler::TriggerManualDump("Texture too large to fit in staging buffer. Increase staging buffer size or do not load this texture.");
                outAllocatedImages.push_back(std::move(newImage));
                continue;
            }

            char* bufferOffset = static_cast<char*>(textureStaging.stagingBuffer.allocationInfo.pMappedData) + allocation.offset;
            memcpy(bufferOffset, stbiData, size);

            VkImageCreateInfo imageCreateInfo = VkHelpers::ImageCreateInfo(VK_FORMAT_R8G8B8A8_UNORM, imagesize,
                                                                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            // transfer src for mipmap only
            newImage = VkResources::CreateAllocatedImage(context, imageCreateInfo);

            VkImageMemoryBarrier2 barrier = VkHelpers::ImageMemoryBarrier(
                newImage.handle,
                VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );
            VkDependencyInfo depInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            depInfo.imageMemoryBarrierCount = 1;
            depInfo.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(textureStaging.commandBuffer, &depInfo);

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = allocation.offset;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = imagesize;

            vkCmdCopyBufferToImage(textureStaging.commandBuffer, textureStaging.stagingBuffer.handle, newImage.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            // todo: mipmapping
            // if (mipmapped) {
            //     vk_helpers::generateMipmaps(cmd, newImage->image, VkExtent2D{newImage->imageExtent.width, newImage->imageExtent.height});
            // }
            // else {
            // }
            barrier = VkHelpers::ImageMemoryBarrier(
                newImage.handle,
                VkHelpers::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT),
                VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            barrier.srcQueueFamilyIndex = context->transferQueueFamily;
            barrier.dstQueueFamilyIndex = context->graphicsQueueFamily;
            vkCmdPipelineBarrier2(textureStaging.commandBuffer, &depInfo);
            newImage.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            stbi_image_free(stbiData);
            stbiData = nullptr;
        }

        outAllocatedImages.push_back(std::move(newImage));
    }


    VK_CHECK(vkEndCommandBuffer(textureStaging.commandBuffer));
    VkCommandBufferSubmitInfo cmdSubmitInfo = VkHelpers::CommandBufferSubmitInfo(textureStaging.commandBuffer);
    const VkSubmitInfo2 submitInfo = VkHelpers::SubmitInfo(&cmdSubmitInfo, nullptr, nullptr);
    VK_CHECK(vkQueueSubmit2(context->transferQueue, 1, &submitInfo, textureStaging.fence));
}

TextureUploadStaging& AssetLoadingThread::GetAvailableTextureStaging()
{
    auto& staging = textureStagingBuffers[currentIndex];

    if (vkGetFenceStatus(context->device, staging.fence) == VK_SUCCESS) {
        staging.stagingAllocator.reset();
        VK_CHECK(vkResetFences(context->device, 1, &staging.fence));
        VK_CHECK(vkResetCommandBuffer(staging.commandBuffer, 0));
        textureStagingGeneration[currentIndex].fetch_add(1);
        return staging;
    }

    // Try next buffer
    currentIndex = (currentIndex + 1) % ASSET_LOAD_TEXTURE_STAGING_COUNT;
    auto& nextStaging = textureStagingBuffers[currentIndex];

    if (vkGetFenceStatus(context->device, nextStaging.fence) == VK_SUCCESS) {
        nextStaging.stagingAllocator.reset();
        VK_CHECK(vkResetFences(context->device, 1, &nextStaging.fence));
        VK_CHECK(vkResetCommandBuffer(nextStaging.commandBuffer, 0));
        textureStagingGeneration[currentIndex].fetch_add(1);
        return nextStaging;
    }

    vkWaitForFences(context->device, 1, &nextStaging.fence, true, UINT64_MAX);
    nextStaging.stagingAllocator.reset();
    VK_CHECK(vkResetFences(context->device, 1, &nextStaging.fence));
    VK_CHECK(vkResetCommandBuffer(nextStaging.commandBuffer, 0));
    textureStagingGeneration[currentIndex].fetch_add(1);
    return nextStaging;
}
}
