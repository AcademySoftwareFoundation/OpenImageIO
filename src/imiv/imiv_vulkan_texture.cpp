// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_types.h"
#include "imiv_vulkan_texture_internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <imgui_impl_vulkan.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

#if defined(IMIV_BACKEND_VULKAN_GLFW)

namespace {

    bool nonblocking_fence_status(VkDevice device, VkFence fence,
                                  const char* context, bool& out_signaled,
                                  std::string& error_message)
    {
        out_signaled = false;
        if (fence == VK_NULL_HANDLE) {
            error_message = Strutil::fmt::format("{} fence is unavailable",
                                                 context);
            return false;
        }

        const VkResult err = vkGetFenceStatus(device, fence);
        if (err == VK_SUCCESS) {
            out_signaled = true;
            return true;
        }
        if (err == VK_NOT_READY)
            return true;

        error_message = Strutil::fmt::format("vkGetFenceStatus failed for {}",
                                             context);
        check_vk_result(err);
        return false;
    }

    bool ensure_texture_upload_submit_resources(VulkanState& vk_state,
                                                VulkanTexture& texture,
                                                std::string& error_message)
    {
        if (texture.upload_command_pool != VK_NULL_HANDLE
            && texture.upload_command_buffer != VK_NULL_HANDLE
            && texture.upload_submit_fence != VK_NULL_HANDLE) {
            return true;
        }

        destroy_texture_upload_submit_resources(vk_state, texture);

        VkResult err                    = VK_SUCCESS;
        VkCommandPoolCreateInfo pool_ci = {};
        pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
                        | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_ci.queueFamilyIndex = vk_state.queue_family;
        err = vkCreateCommandPool(vk_state.device, &pool_ci, vk_state.allocator,
                                  &texture.upload_command_pool);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateCommandPool failed for upload async submit";
            destroy_texture_upload_submit_resources(vk_state, texture);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_POOL,
                           texture.upload_command_pool,
                           "imiv.upload_async.command_pool");

        VkCommandBufferAllocateInfo command_alloc = {};
        command_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_alloc.commandPool        = texture.upload_command_pool;
        command_alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_alloc.commandBufferCount = 1;
        err = vkAllocateCommandBuffers(vk_state.device, &command_alloc,
                                       &texture.upload_command_buffer);
        if (err != VK_SUCCESS) {
            error_message = "vkAllocateCommandBuffers failed for upload async "
                            "submit";
            destroy_texture_upload_submit_resources(vk_state, texture);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_BUFFER,
                           texture.upload_command_buffer,
                           "imiv.upload_async.command_buffer");

        VkFenceCreateInfo fence_ci = {};
        fence_ci.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_ci.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
        err = vkCreateFence(vk_state.device, &fence_ci, vk_state.allocator,
                            &texture.upload_submit_fence);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateFence failed for upload async submit";
            destroy_texture_upload_submit_resources(vk_state, texture);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_FENCE,
                           texture.upload_submit_fence,
                           "imiv.upload_async.fence");
        return true;
    }

}  // namespace

void
destroy_texture_upload_submit_resources(VulkanState& vk_state,
                                        VulkanTexture& texture)
{
    if (texture.upload_compute_set != VK_NULL_HANDLE
        && vk_state.compute_descriptor_pool != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(vk_state.device, vk_state.compute_descriptor_pool,
                             1, &texture.upload_compute_set);
        texture.upload_compute_set = VK_NULL_HANDLE;
    }
    if (texture.upload_source_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk_state.device, texture.upload_source_buffer,
                        vk_state.allocator);
        texture.upload_source_buffer = VK_NULL_HANDLE;
    }
    if (texture.upload_source_memory != VK_NULL_HANDLE) {
        vkFreeMemory(vk_state.device, texture.upload_source_memory,
                     vk_state.allocator);
        texture.upload_source_memory = VK_NULL_HANDLE;
    }
    if (texture.upload_staging_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk_state.device, texture.upload_staging_buffer,
                        vk_state.allocator);
        texture.upload_staging_buffer = VK_NULL_HANDLE;
    }
    if (texture.upload_staging_memory != VK_NULL_HANDLE) {
        vkFreeMemory(vk_state.device, texture.upload_staging_memory,
                     vk_state.allocator);
        texture.upload_staging_memory = VK_NULL_HANDLE;
    }
    if (texture.upload_submit_fence != VK_NULL_HANDLE) {
        vkDestroyFence(vk_state.device, texture.upload_submit_fence,
                       vk_state.allocator);
        texture.upload_submit_fence = VK_NULL_HANDLE;
    }
    texture.upload_command_buffer = VK_NULL_HANDLE;
    if (texture.upload_command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vk_state.device, texture.upload_command_pool,
                             vk_state.allocator);
        texture.upload_command_pool = VK_NULL_HANDLE;
    }
    texture.upload_submit_pending = false;
}

bool
poll_texture_upload_submission(VulkanState& vk_state, VulkanTexture& texture,
                               bool wait_for_completion,
                               std::string& error_message)
{
    if (texture.source_ready)
        return true;
    if (!texture.upload_submit_pending)
        return false;
    if (texture.upload_submit_fence == VK_NULL_HANDLE) {
        texture.upload_submit_pending = false;
        error_message                 = "upload submit fence is unavailable";
        return false;
    }

    VkResult err = VK_SUCCESS;
    if (wait_for_completion) {
        err = vkWaitForFences(vk_state.device, 1, &texture.upload_submit_fence,
                              VK_TRUE, UINT64_MAX);
    } else {
        err = vkGetFenceStatus(vk_state.device, texture.upload_submit_fence);
        if (err == VK_NOT_READY)
            return false;
    }
    if (err != VK_SUCCESS) {
        error_message = wait_for_completion
                            ? "vkWaitForFences failed for upload async submit"
                            : "vkGetFenceStatus failed for upload async submit";
        check_vk_result(err);
        return false;
    }

    texture.upload_submit_pending = false;
    texture.source_ready          = true;
    texture.preview_dirty         = true;
    destroy_texture_upload_submit_resources(vk_state, texture);
    return true;
}

void
check_vk_result(VkResult err)
{
    if (err == VK_SUCCESS)
        return;
    print(stderr, "imiv: Vulkan error {}\n", static_cast<int>(err));
    if (err == VK_ERROR_DEVICE_LOST || err == VK_ERROR_OUT_OF_DEVICE_MEMORY
        || err == VK_ERROR_OUT_OF_HOST_MEMORY
        || err == VK_ERROR_INITIALIZATION_FAILED) {
        print(stderr,
              "imiv: fatal Vulkan error {}; aborting to avoid undefined "
              "behavior\n",
              static_cast<int>(err));
        std::abort();
    }
}

bool
find_memory_type(VkPhysicalDevice physical_device, uint32_t type_bits,
                 VkMemoryPropertyFlags required, uint32_t& memory_type_index)
{
    VkPhysicalDeviceMemoryProperties memory_properties = {};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
        const bool type_matches = (type_bits & (1u << i)) != 0;
        const bool has_flags = (memory_properties.memoryTypes[i].propertyFlags
                                & required)
                               == required;
        if (type_matches && has_flags) {
            memory_type_index = i;
            return true;
        }
    }
    return false;
}

bool
find_memory_type_with_fallback(VkPhysicalDevice physical_device,
                               uint32_t type_bits,
                               VkMemoryPropertyFlags preferred,
                               uint32_t& memory_type_index)
{
    if (find_memory_type(physical_device, type_bits, preferred,
                         memory_type_index)) {
        return true;
    }
    return find_memory_type(physical_device, type_bits, 0, memory_type_index);
}

void
destroy_texture(VulkanState& vk_state, VulkanTexture& texture)
{
    if (texture.upload_submit_pending
        && texture.upload_submit_fence != VK_NULL_HANDLE) {
        VkResult err = vkWaitForFences(vk_state.device, 1,
                                       &texture.upload_submit_fence, VK_TRUE,
                                       UINT64_MAX);
        check_vk_result(err);
        texture.upload_submit_pending = false;
    }
    if (texture.preview_submit_pending
        && texture.preview_submit_fence != VK_NULL_HANDLE) {
        VkResult err = vkWaitForFences(vk_state.device, 1,
                                       &texture.preview_submit_fence, VK_TRUE,
                                       UINT64_MAX);
        check_vk_result(err);
        texture.preview_submit_pending = false;
    }
    if (texture.pixelview_set != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(texture.pixelview_set);
        texture.pixelview_set = VK_NULL_HANDLE;
    }
    if (texture.nearest_mag_set != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(texture.nearest_mag_set);
        texture.nearest_mag_set = VK_NULL_HANDLE;
    }
    if (texture.set != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(texture.set);
        texture.set = VK_NULL_HANDLE;
    }
    if (texture.preview_source_set != VK_NULL_HANDLE
        && vk_state.preview_descriptor_pool != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(vk_state.device, vk_state.preview_descriptor_pool,
                             1, &texture.preview_source_set);
        texture.preview_source_set = VK_NULL_HANDLE;
    }
    if (texture.preview_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(vk_state.device, texture.preview_framebuffer,
                             vk_state.allocator);
        texture.preview_framebuffer = VK_NULL_HANDLE;
    }
    if (texture.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(vk_state.device, texture.sampler, vk_state.allocator);
        texture.sampler = VK_NULL_HANDLE;
    }
    if (texture.nearest_mag_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(vk_state.device, texture.nearest_mag_sampler,
                         vk_state.allocator);
        texture.nearest_mag_sampler = VK_NULL_HANDLE;
    }
    if (texture.pixelview_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(vk_state.device, texture.pixelview_sampler,
                         vk_state.allocator);
        texture.pixelview_sampler = VK_NULL_HANDLE;
    }
    destroy_texture_upload_submit_resources(vk_state, texture);
    destroy_texture_preview_submit_resources(vk_state, texture);
    if (texture.view != VK_NULL_HANDLE) {
        vkDestroyImageView(vk_state.device, texture.view, vk_state.allocator);
        texture.view = VK_NULL_HANDLE;
    }
    if (texture.image != VK_NULL_HANDLE) {
        vkDestroyImage(vk_state.device, texture.image, vk_state.allocator);
        texture.image = VK_NULL_HANDLE;
    }
    if (texture.memory != VK_NULL_HANDLE) {
        vkFreeMemory(vk_state.device, texture.memory, vk_state.allocator);
        texture.memory = VK_NULL_HANDLE;
    }
    if (texture.source_view != VK_NULL_HANDLE) {
        vkDestroyImageView(vk_state.device, texture.source_view,
                           vk_state.allocator);
        texture.source_view = VK_NULL_HANDLE;
    }
    if (texture.source_image != VK_NULL_HANDLE) {
        vkDestroyImage(vk_state.device, texture.source_image,
                       vk_state.allocator);
        texture.source_image = VK_NULL_HANDLE;
    }
    if (texture.source_memory != VK_NULL_HANDLE) {
        vkFreeMemory(vk_state.device, texture.source_memory,
                     vk_state.allocator);
        texture.source_memory = VK_NULL_HANDLE;
    }
    texture.width                = 0;
    texture.height               = 0;
    texture.source_ready         = false;
    texture.preview_initialized  = false;
    texture.preview_dirty        = false;
    texture.preview_params_valid = false;
}

bool
create_texture(VulkanState& vk_state, const LoadedImage& image,
               VulkanTexture& texture, std::string& error_message)
{
    destroy_texture(vk_state, texture);

    if (!vk_state.compute_upload_ready) {
        error_message = "compute upload path is not initialized";
        return false;
    }
    if (image.width <= 0 || image.height <= 0 || image.pixels.empty()) {
        error_message = "invalid image data for texture upload";
        return false;
    }
    if (vk_state.max_image_dimension_2d > 0
        && (static_cast<uint32_t>(image.width) > vk_state.max_image_dimension_2d
            || static_cast<uint32_t>(image.height)
                   > vk_state.max_image_dimension_2d)) {
        error_message = Strutil::fmt::format(
            "image dimensions {}x{} exceed device maxImageDimension2D {}",
            image.width, image.height, vk_state.max_image_dimension_2d);
        return false;
    }

    UploadDataType upload_type = image.type;
    size_t channel_bytes       = image.channel_bytes;
    size_t row_pitch_bytes     = image.row_pitch_bytes;
    const int channel_count    = std::max(0, image.nchannels);
    if (channel_count <= 0) {
        error_message = "invalid source channel count";
        return false;
    }

    std::vector<unsigned char> converted_pixels;
    const unsigned char* upload_ptr = image.pixels.data();
    size_t upload_bytes             = image.pixels.size();
    bool use_fp64_pipeline          = false;

    if (upload_type == UploadDataType::Double) {
        if (vk_state.compute_pipeline_fp64 != VK_NULL_HANDLE) {
            use_fp64_pipeline = true;
            if (vk_state.verbose_logging) {
                print("imiv: using fp64 compute upload path for '{}'\n",
                      image.path);
            }
        } else {
            const size_t value_count = image.pixels.size() / sizeof(double);
            converted_pixels.resize(value_count * sizeof(float));
            const double* src = reinterpret_cast<const double*>(
                image.pixels.data());
            float* dst = reinterpret_cast<float*>(converted_pixels.data());
            for (size_t i = 0; i < value_count; ++i)
                dst[i] = static_cast<float>(src[i]);
            upload_type     = UploadDataType::Float;
            channel_bytes   = sizeof(float);
            row_pitch_bytes = static_cast<size_t>(image.width)
                              * static_cast<size_t>(channel_count)
                              * channel_bytes;
            upload_ptr   = converted_pixels.data();
            upload_bytes = converted_pixels.size();
            print(stderr, "imiv: fp64 compute pipeline unavailable; converting "
                          "double input to float on CPU\n");
        }
    }

    const size_t pixel_stride_bytes = channel_bytes
                                      * static_cast<size_t>(channel_count);
    if (pixel_stride_bytes == 0 || row_pitch_bytes == 0
        || row_pitch_bytes
               < static_cast<size_t>(image.width) * pixel_stride_bytes) {
        error_message = "invalid source stride for compute upload";
        return false;
    }

    const VkDeviceSize upload_size_aligned = static_cast<VkDeviceSize>(
        (upload_bytes + 3u) & ~size_t(3));

    VkBuffer staging_buffer        = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory  = VK_NULL_HANDLE;
    VkBuffer source_buffer         = VK_NULL_HANDLE;
    VkDeviceMemory source_memory   = VK_NULL_HANDLE;
    VkDescriptorSet compute_set    = VK_NULL_HANDLE;
    VkCommandBuffer upload_command = VK_NULL_HANDLE;
    bool ok                        = false;

    do {
        VkResult err = VK_SUCCESS;

        VkImageCreateInfo source_ci = {};
        source_ci.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        source_ci.imageType         = VK_IMAGE_TYPE_2D;
        source_ci.format            = vk_state.compute_output_format;
        source_ci.extent.width      = static_cast<uint32_t>(image.width);
        source_ci.extent.height     = static_cast<uint32_t>(image.height);
        source_ci.extent.depth      = 1;
        source_ci.mipLevels         = 1;
        source_ci.arrayLayers       = 1;
        source_ci.samples           = VK_SAMPLE_COUNT_1_BIT;
        source_ci.tiling            = VK_IMAGE_TILING_OPTIMAL;
        source_ci.usage             = VK_IMAGE_USAGE_STORAGE_BIT
                          | VK_IMAGE_USAGE_SAMPLED_BIT;
        source_ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        source_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        err = vkCreateImage(vk_state.device, &source_ci, vk_state.allocator,
                            &texture.source_image);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateImage failed for source image";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE, texture.source_image,
                           "imiv.viewer.source_image");

        VkMemoryRequirements source_reqs = {};
        vkGetImageMemoryRequirements(vk_state.device, texture.source_image,
                                     &source_reqs);

        uint32_t image_memory_type = 0;
        if (!find_memory_type_with_fallback(vk_state.physical_device,
                                            source_reqs.memoryTypeBits,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            image_memory_type)) {
            error_message = "no compatible memory type for source image";
            break;
        }

        VkMemoryAllocateInfo source_alloc = {};
        source_alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        source_alloc.allocationSize  = source_reqs.size;
        source_alloc.memoryTypeIndex = image_memory_type;
        err = vkAllocateMemory(vk_state.device, &source_alloc,
                               vk_state.allocator, &texture.source_memory);
        if (err != VK_SUCCESS) {
            error_message = "vkAllocateMemory failed for source image";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                           texture.source_memory,
                           "imiv.viewer.source_image.memory");
        err = vkBindImageMemory(vk_state.device, texture.source_image,
                                texture.source_memory, 0);
        if (err != VK_SUCCESS) {
            error_message = "vkBindImageMemory failed for source image";
            break;
        }

        VkImageViewCreateInfo source_view_ci = {};
        source_view_ci.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        source_view_ci.image        = texture.source_image;
        source_view_ci.viewType     = VK_IMAGE_VIEW_TYPE_2D;
        source_view_ci.format       = vk_state.compute_output_format;
        source_view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        source_view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        source_view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        source_view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        source_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        source_view_ci.subresourceRange.baseMipLevel   = 0;
        source_view_ci.subresourceRange.levelCount     = 1;
        source_view_ci.subresourceRange.baseArrayLayer = 0;
        source_view_ci.subresourceRange.layerCount     = 1;
        err = vkCreateImageView(vk_state.device, &source_view_ci,
                                vk_state.allocator, &texture.source_view);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateImageView failed for source image";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE_VIEW,
                           texture.source_view, "imiv.viewer.source_view");

        VkImageCreateInfo preview_ci = source_ci;
        preview_ci.usage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                           | VK_IMAGE_USAGE_SAMPLED_BIT;
        err = vkCreateImage(vk_state.device, &preview_ci, vk_state.allocator,
                            &texture.image);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateImage failed for preview image";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE, texture.image,
                           "imiv.viewer.preview_image");

        VkMemoryRequirements preview_reqs = {};
        vkGetImageMemoryRequirements(vk_state.device, texture.image,
                                     &preview_reqs);
        uint32_t preview_memory_type = 0;
        if (!find_memory_type_with_fallback(vk_state.physical_device,
                                            preview_reqs.memoryTypeBits,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            preview_memory_type)) {
            error_message = "no compatible memory type for preview image";
            break;
        }
        VkMemoryAllocateInfo preview_alloc = {};
        preview_alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        preview_alloc.allocationSize  = preview_reqs.size;
        preview_alloc.memoryTypeIndex = preview_memory_type;
        err = vkAllocateMemory(vk_state.device, &preview_alloc,
                               vk_state.allocator, &texture.memory);
        if (err != VK_SUCCESS) {
            error_message = "vkAllocateMemory failed for preview image";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                           texture.memory, "imiv.viewer.preview_image.memory");
        err = vkBindImageMemory(vk_state.device, texture.image, texture.memory,
                                0);
        if (err != VK_SUCCESS) {
            error_message = "vkBindImageMemory failed for preview image";
            break;
        }

        VkImageViewCreateInfo preview_view_ci = source_view_ci;
        preview_view_ci.image                 = texture.image;
        err = vkCreateImageView(vk_state.device, &preview_view_ci,
                                vk_state.allocator, &texture.view);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateImageView failed for preview image";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE_VIEW, texture.view,
                           "imiv.viewer.preview_view");

        VkFramebufferCreateInfo fb_ci = {};
        fb_ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_ci.renderPass      = vk_state.preview_render_pass;
        fb_ci.attachmentCount = 1;
        fb_ci.pAttachments    = &texture.view;
        fb_ci.width           = static_cast<uint32_t>(image.width);
        fb_ci.height          = static_cast<uint32_t>(image.height);
        fb_ci.layers          = 1;
        err = vkCreateFramebuffer(vk_state.device, &fb_ci, vk_state.allocator,
                                  &texture.preview_framebuffer);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateFramebuffer failed for preview image";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_FRAMEBUFFER,
                           texture.preview_framebuffer,
                           "imiv.viewer.preview_framebuffer");

        VkBufferCreateInfo buffer_ci = {};
        buffer_ci.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_ci.size               = upload_size_aligned;
        buffer_ci.usage              = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                          | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        err = vkCreateBuffer(vk_state.device, &buffer_ci, vk_state.allocator,
                             &source_buffer);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateBuffer failed for source buffer";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_BUFFER, source_buffer,
                           "imiv.viewer.upload.source_buffer");

        VkMemoryRequirements staging_reqs = {};
        vkGetBufferMemoryRequirements(vk_state.device, source_buffer,
                                      &staging_reqs);

        uint32_t staging_memory_type = 0;
        if (!find_memory_type_with_fallback(vk_state.physical_device,
                                            staging_reqs.memoryTypeBits,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            staging_memory_type)) {
            error_message = "no compatible memory type for source buffer";
            break;
        }

        VkMemoryAllocateInfo staging_alloc = {};
        staging_alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        staging_alloc.allocationSize  = staging_reqs.size;
        staging_alloc.memoryTypeIndex = staging_memory_type;
        err = vkAllocateMemory(vk_state.device, &staging_alloc,
                               vk_state.allocator, &source_memory);
        if (err != VK_SUCCESS) {
            error_message = "vkAllocateMemory failed for source buffer";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                           source_memory, "imiv.viewer.upload.source_memory");
        err = vkBindBufferMemory(vk_state.device, source_buffer, source_memory,
                                 0);
        if (err != VK_SUCCESS) {
            error_message = "vkBindBufferMemory failed for source buffer";
            break;
        }

        VkBufferCreateInfo staging_ci = {};
        staging_ci.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        staging_ci.size               = upload_size_aligned;
        staging_ci.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        staging_ci.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
        err = vkCreateBuffer(vk_state.device, &staging_ci, vk_state.allocator,
                             &staging_buffer);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateBuffer failed for staging buffer";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_BUFFER, staging_buffer,
                           "imiv.viewer.upload.staging_buffer");

        VkMemoryRequirements staging_buffer_reqs = {};
        vkGetBufferMemoryRequirements(vk_state.device, staging_buffer,
                                      &staging_buffer_reqs);
        uint32_t host_visible_memory_type = 0;
        if (!find_memory_type(vk_state.physical_device,
                              staging_buffer_reqs.memoryTypeBits,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              host_visible_memory_type)) {
            error_message = "no host-visible memory type for staging buffer";
            break;
        }
        VkMemoryAllocateInfo host_alloc = {};
        host_alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        host_alloc.allocationSize  = staging_buffer_reqs.size;
        host_alloc.memoryTypeIndex = host_visible_memory_type;
        err = vkAllocateMemory(vk_state.device, &host_alloc, vk_state.allocator,
                               &staging_memory);
        if (err != VK_SUCCESS) {
            error_message = "vkAllocateMemory failed for staging buffer";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                           staging_memory, "imiv.viewer.upload.staging_memory");
        err = vkBindBufferMemory(vk_state.device, staging_buffer,
                                 staging_memory, 0);
        if (err != VK_SUCCESS) {
            error_message = "vkBindBufferMemory failed for staging buffer";
            break;
        }

        void* mapped = nullptr;
        err          = vkMapMemory(vk_state.device, staging_memory, 0,
                                   upload_size_aligned, 0, &mapped);
        if (err != VK_SUCCESS || mapped == nullptr) {
            error_message = "vkMapMemory failed for staging buffer";
            break;
        }
        std::memset(mapped, 0, static_cast<size_t>(upload_size_aligned));
        std::memcpy(mapped, upload_ptr, upload_bytes);
        vkUnmapMemory(vk_state.device, staging_memory);

        VkDescriptorSetAllocateInfo set_alloc = {};
        set_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        set_alloc.descriptorPool     = vk_state.compute_descriptor_pool;
        set_alloc.descriptorSetCount = 1;
        set_alloc.pSetLayouts        = &vk_state.compute_descriptor_set_layout;
        err = vkAllocateDescriptorSets(vk_state.device, &set_alloc,
                                       &compute_set);
        if (err != VK_SUCCESS) {
            error_message = "vkAllocateDescriptorSets failed for upload compute";
            break;
        }

        if (!ensure_texture_upload_submit_resources(vk_state, texture,
                                                    error_message)) {
            break;
        }

        bool upload_fence_signaled = false;
        if (!nonblocking_fence_status(vk_state.device,
                                      texture.upload_submit_fence,
                                      "upload async submit",
                                      upload_fence_signaled, error_message)) {
            break;
        }
        if (!upload_fence_signaled) {
            error_message = "upload async submit is still in flight during "
                            "texture initialization";
            break;
        }
        err = vkResetCommandPool(vk_state.device, texture.upload_command_pool,
                                 0);
        if (err != VK_SUCCESS) {
            error_message = "vkResetCommandPool failed for upload async submit";
            break;
        }

        upload_command                 = texture.upload_command_buffer;
        VkCommandBufferBeginInfo begin = {};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err         = vkBeginCommandBuffer(upload_command, &begin);
        if (err != VK_SUCCESS) {
            error_message = "vkBeginCommandBuffer failed for upload update";
            break;
        }

        VkDescriptorBufferInfo source_buffer_info = {};
        source_buffer_info.buffer                 = source_buffer;
        source_buffer_info.offset                 = 0;
        source_buffer_info.range                  = upload_size_aligned;

        VkDescriptorImageInfo output_image_info = {};
        output_image_info.imageView             = texture.source_view;
        output_image_info.imageLayout           = VK_IMAGE_LAYOUT_GENERAL;
        output_image_info.sampler               = VK_NULL_HANDLE;

        VkWriteDescriptorSet writes[2] = {};
        writes[0].sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet               = compute_set;
        writes[0].dstBinding           = 0;
        writes[0].descriptorCount      = 1;
        writes[0].descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[0].pBufferInfo          = &source_buffer_info;
        writes[1].sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet               = compute_set;
        writes[1].dstBinding           = 1;
        writes[1].descriptorCount      = 1;
        writes[1].descriptorType       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].pImageInfo           = &output_image_info;
        vkUpdateDescriptorSets(vk_state.device, 2, writes, 0, nullptr);

        VkSamplerCreateInfo sampler_ci  = {};
        sampler_ci.sType                = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        VkFormatProperties output_props = {};
        vkGetPhysicalDeviceFormatProperties(vk_state.physical_device,
                                            vk_state.compute_output_format,
                                            &output_props);
        const bool has_linear_filter
            = (output_props.optimalTilingFeatures
               & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
              != 0;
        sampler_ci.magFilter     = has_linear_filter ? VK_FILTER_LINEAR
                                                     : VK_FILTER_NEAREST;
        sampler_ci.minFilter     = has_linear_filter ? VK_FILTER_LINEAR
                                                     : VK_FILTER_NEAREST;
        sampler_ci.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_ci.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_ci.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_ci.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_ci.minLod        = 0.0f;
        sampler_ci.maxLod        = 1000.0f;
        sampler_ci.maxAnisotropy = 1.0f;
        err = vkCreateSampler(vk_state.device, &sampler_ci, vk_state.allocator,
                              &texture.sampler);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateSampler failed";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_SAMPLER, texture.sampler,
                           "imiv.viewer.sampler");

        VkSamplerCreateInfo pixelview_sampler_ci   = sampler_ci;
        VkSamplerCreateInfo nearest_mag_sampler_ci = sampler_ci;
        nearest_mag_sampler_ci.magFilter           = VK_FILTER_NEAREST;
        pixelview_sampler_ci.magFilter             = VK_FILTER_NEAREST;
        pixelview_sampler_ci.minFilter             = VK_FILTER_NEAREST;
        pixelview_sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        err = vkCreateSampler(vk_state.device, &nearest_mag_sampler_ci,
                              vk_state.allocator, &texture.nearest_mag_sampler);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateSampler failed for nearest-mag view";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_SAMPLER,
                           texture.nearest_mag_sampler,
                           "imiv.viewer.nearest_mag_sampler");
        err = vkCreateSampler(vk_state.device, &pixelview_sampler_ci,
                              vk_state.allocator, &texture.pixelview_sampler);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateSampler failed for pixel closeup";
            break;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_SAMPLER,
                           texture.pixelview_sampler,
                           "imiv.viewer.pixelview_sampler");

        VkDescriptorSetAllocateInfo preview_set_alloc = {};
        preview_set_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        preview_set_alloc.descriptorPool     = vk_state.preview_descriptor_pool;
        preview_set_alloc.descriptorSetCount = 1;
        preview_set_alloc.pSetLayouts = &vk_state.preview_descriptor_set_layout;
        err = vkAllocateDescriptorSets(vk_state.device, &preview_set_alloc,
                                       &texture.preview_source_set);
        if (err != VK_SUCCESS) {
            error_message
                = "vkAllocateDescriptorSets failed for preview source set";
            break;
        }
        VkDescriptorImageInfo preview_source_image = {};
        preview_source_image.sampler               = texture.sampler;
        preview_source_image.imageView             = texture.source_view;
        preview_source_image.imageLayout
            = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet preview_write = {};
        preview_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        preview_write.dstSet          = texture.preview_source_set;
        preview_write.dstBinding      = 0;
        preview_write.descriptorCount = 1;
        preview_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        preview_write.pImageInfo = &preview_source_image;
        vkUpdateDescriptorSets(vk_state.device, 1, &preview_write, 0, nullptr);

        VkBufferCopy copy_region = {};
        copy_region.srcOffset    = 0;
        copy_region.dstOffset    = 0;
        copy_region.size         = upload_size_aligned;
        vkCmdCopyBuffer(upload_command, staging_buffer, source_buffer, 1,
                        &copy_region);

        VkBufferMemoryBarrier source_to_compute = {};
        source_to_compute.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        source_to_compute.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_to_compute.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
        source_to_compute.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        source_to_compute.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        source_to_compute.buffer              = source_buffer;
        source_to_compute.offset              = 0;
        source_to_compute.size                = upload_size_aligned;

        VkImageMemoryBarrier image_to_general = {};
        image_to_general.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_to_general.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_to_general.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        image_to_general.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_to_general.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_to_general.image               = texture.source_image;
        image_to_general.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_to_general.subresourceRange.baseMipLevel   = 0;
        image_to_general.subresourceRange.levelCount     = 1;
        image_to_general.subresourceRange.baseArrayLayer = 0;
        image_to_general.subresourceRange.layerCount     = 1;
        image_to_general.srcAccessMask                   = 0;
        image_to_general.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(upload_command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                             nullptr, 1, &source_to_compute, 1,
                             &image_to_general);

        vkCmdBindPipeline(upload_command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          use_fp64_pipeline ? vk_state.compute_pipeline_fp64
                                            : vk_state.compute_pipeline);
        vkCmdBindDescriptorSets(upload_command, VK_PIPELINE_BIND_POINT_COMPUTE,
                                vk_state.compute_pipeline_layout, 0, 1,
                                &compute_set, 0, nullptr);

        UploadComputePushConstants push = {};
        push.width                      = static_cast<uint32_t>(image.width);
        push.height                     = static_cast<uint32_t>(image.height);
        push.row_pitch_bytes = static_cast<uint32_t>(row_pitch_bytes);
        push.pixel_stride    = static_cast<uint32_t>(pixel_stride_bytes);
        push.channel_count   = static_cast<uint32_t>(channel_count);
        push.data_type       = static_cast<uint32_t>(upload_type);
        vkCmdPushConstants(upload_command, vk_state.compute_pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);

        const uint32_t group_x = (push.width + 15u) / 16u;
        const uint32_t group_y = (push.height + 15u) / 16u;
        vkCmdDispatch(upload_command, group_x, group_y, 1);

        VkImageMemoryBarrier to_shader = {};
        to_shader.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_shader.oldLayout            = VK_IMAGE_LAYOUT_GENERAL;
        to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        to_shader.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        to_shader.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        to_shader.image                           = texture.source_image;
        to_shader.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        to_shader.subresourceRange.baseMipLevel   = 0;
        to_shader.subresourceRange.levelCount     = 1;
        to_shader.subresourceRange.baseArrayLayer = 0;
        to_shader.subresourceRange.layerCount     = 1;
        to_shader.srcAccessMask                   = VK_ACCESS_SHADER_WRITE_BIT;
        to_shader.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(upload_command,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &to_shader);

        err = vkEndCommandBuffer(upload_command);
        if (err != VK_SUCCESS) {
            error_message = "vkEndCommandBuffer failed for upload update";
            break;
        }
        err = vkResetFences(vk_state.device, 1, &texture.upload_submit_fence);
        if (err != VK_SUCCESS) {
            error_message = "vkResetFences failed for upload async submit";
            break;
        }

        VkSubmitInfo submit       = {};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &upload_command;
        err                       = vkQueueSubmit(vk_state.queue, 1, &submit,
                                                  texture.upload_submit_fence);
        if (err != VK_SUCCESS) {
            error_message = "vkQueueSubmit failed for upload update";
            break;
        }
        upload_command = VK_NULL_HANDLE;

        texture.upload_staging_buffer = staging_buffer;
        texture.upload_staging_memory = staging_memory;
        texture.upload_source_buffer  = source_buffer;
        texture.upload_source_memory  = source_memory;
        texture.upload_compute_set    = compute_set;
        texture.upload_submit_pending = true;
        staging_buffer                = VK_NULL_HANDLE;
        staging_memory                = VK_NULL_HANDLE;
        source_buffer                 = VK_NULL_HANDLE;
        source_memory                 = VK_NULL_HANDLE;
        compute_set                   = VK_NULL_HANDLE;

        texture.set = ImGui_ImplVulkan_AddTexture(
            texture.sampler, texture.view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (texture.set == VK_NULL_HANDLE) {
            error_message = "ImGui_ImplVulkan_AddTexture failed";
            break;
        }
        texture.nearest_mag_set = ImGui_ImplVulkan_AddTexture(
            texture.nearest_mag_sampler, texture.view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (texture.nearest_mag_set == VK_NULL_HANDLE) {
            error_message
                = "ImGui_ImplVulkan_AddTexture failed for nearest-mag "
                  "view";
            break;
        }
        texture.pixelview_set = ImGui_ImplVulkan_AddTexture(
            texture.pixelview_sampler, texture.view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (texture.pixelview_set == VK_NULL_HANDLE) {
            error_message
                = "ImGui_ImplVulkan_AddTexture failed for pixel closeup";
            break;
        }

        texture.width                = image.width;
        texture.height               = image.height;
        texture.source_ready         = false;
        texture.preview_initialized  = false;
        texture.preview_dirty        = true;
        texture.preview_params_valid = false;
        ok                           = true;

    } while (false);

    if (compute_set != VK_NULL_HANDLE)
        vkFreeDescriptorSets(vk_state.device, vk_state.compute_descriptor_pool,
                             1, &compute_set);
    if (source_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(vk_state.device, source_buffer, vk_state.allocator);
    if (source_memory != VK_NULL_HANDLE)
        vkFreeMemory(vk_state.device, source_memory, vk_state.allocator);
    if (staging_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(vk_state.device, staging_buffer, vk_state.allocator);
    if (staging_memory != VK_NULL_HANDLE)
        vkFreeMemory(vk_state.device, staging_memory, vk_state.allocator);
    if (!ok)
        destroy_texture(vk_state, texture);
    return ok;
}

#endif

}  // namespace Imiv
