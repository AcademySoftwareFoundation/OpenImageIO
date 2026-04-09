// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_loaded_image.h"
#include "imiv_parse.h"
#include "imiv_tiling.h"
#include "imiv_vulkan_resource_utils.h"
#include "imiv_vulkan_texture_internal.h"
#include "imiv_vulkan_types.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <imgui_impl_vulkan.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

#if defined(IMIV_WITH_VULKAN)

namespace {

    bool upload_stage_logging_enabled(const VulkanState& vk_state)
    {
        return vk_state.verbose_logging
               || env_flag_is_truthy("IMIV_VULKAN_UPLOAD_STAGE_LOG");
    }

    void log_upload_stage(const VulkanState& vk_state,
                          const VulkanTexture& texture, const char* stage,
                          const std::string& details = {})
    {
        if (!upload_stage_logging_enabled(vk_state))
            return;
        if (details.empty()) {
            print(stderr, "imiv: Vulkan upload stage [{}] '{}'\n", stage,
                  texture.debug_label);
            return;
        }
        print(stderr, "imiv: Vulkan upload stage [{}] '{}' {}\n", stage,
              texture.debug_label, details);
    }

    bool texture_has_allocated_resources(const VulkanTexture& texture)
    {
        return texture.source_image != VK_NULL_HANDLE
               || texture.source_view != VK_NULL_HANDLE
               || texture.source_memory != VK_NULL_HANDLE
               || texture.image != VK_NULL_HANDLE
               || texture.view != VK_NULL_HANDLE
               || texture.memory != VK_NULL_HANDLE
               || texture.preview_framebuffer != VK_NULL_HANDLE
               || texture.preview_source_set != VK_NULL_HANDLE
               || texture.sampler != VK_NULL_HANDLE
               || texture.nearest_mag_sampler != VK_NULL_HANDLE
               || texture.pixelview_sampler != VK_NULL_HANDLE
               || texture.set != VK_NULL_HANDLE
               || texture.nearest_mag_set != VK_NULL_HANDLE
               || texture.pixelview_set != VK_NULL_HANDLE
               || texture.upload_staging_buffer != VK_NULL_HANDLE
               || texture.upload_staging_memory != VK_NULL_HANDLE
               || texture.upload_source_buffer != VK_NULL_HANDLE
               || texture.upload_source_memory != VK_NULL_HANDLE
               || texture.upload_compute_set != VK_NULL_HANDLE
               || texture.upload_command_pool != VK_NULL_HANDLE
               || texture.upload_command_buffer != VK_NULL_HANDLE
               || texture.upload_submit_fence != VK_NULL_HANDLE
               || texture.preview_command_pool != VK_NULL_HANDLE
               || texture.preview_command_buffer != VK_NULL_HANDLE
               || texture.preview_submit_fence != VK_NULL_HANDLE;
    }

    void destroy_texture_now(VulkanState& vk_state, VulkanTexture& texture)
    {
        if (!texture_has_allocated_resources(texture)) {
            texture.width  = 0;
            texture.height = 0;
            texture.debug_label.clear();
            texture.source_ready         = false;
            texture.preview_initialized  = false;
            texture.preview_dirty        = false;
            texture.preview_params_valid = false;
            return;
        }

        if (vk_state.verbose_logging) {
            print("imiv: Vulkan texture destroy-now '{}'\n",
                  texture.debug_label);
        }

        if (texture.upload_submit_pending
            && texture.upload_submit_fence != VK_NULL_HANDLE) {
            VkResult err = vkWaitForFences(vk_state.device, 1,
                                           &texture.upload_submit_fence,
                                           VK_TRUE, UINT64_MAX);
            check_vk_result(err);
            texture.upload_submit_pending = false;
        }
        if (texture.preview_submit_pending
            && texture.preview_submit_fence != VK_NULL_HANDLE) {
            VkResult err = vkWaitForFences(vk_state.device, 1,
                                           &texture.preview_submit_fence,
                                           VK_TRUE, UINT64_MAX);
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
            vkFreeDescriptorSets(vk_state.device,
                                 vk_state.preview_descriptor_pool, 1,
                                 &texture.preview_source_set);
            texture.preview_source_set = VK_NULL_HANDLE;
        }
        if (texture.preview_framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(vk_state.device, texture.preview_framebuffer,
                                 vk_state.allocator);
            texture.preview_framebuffer = VK_NULL_HANDLE;
        }
        if (texture.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(vk_state.device, texture.sampler,
                             vk_state.allocator);
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
            vkDestroyImageView(vk_state.device, texture.view,
                               vk_state.allocator);
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
        texture.width  = 0;
        texture.height = 0;
        texture.debug_label.clear();
        texture.source_ready         = false;
        texture.preview_initialized  = false;
        texture.preview_dirty        = false;
        texture.preview_params_valid = false;
    }

    bool ensure_texture_upload_submit_resources(VulkanState& vk_state,
                                                VulkanTexture& texture,
                                                std::string& error_message)
    {
        return ensure_async_submit_resources(
            vk_state, texture.upload_command_pool,
            texture.upload_command_buffer, texture.upload_submit_fence,
            "vkCreateCommandPool failed for upload async submit",
            "vkAllocateCommandBuffers failed for upload async submit",
            "vkCreateFence failed for upload async submit",
            "imiv.upload_async.command_pool",
            "imiv.upload_async.command_buffer", "imiv.upload_async.fence",
            error_message);
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
    destroy_async_submit_resources(vk_state, texture.upload_command_pool,
                                   texture.upload_command_buffer,
                                   texture.upload_submit_fence);
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
        log_upload_stage(vk_state, texture, "wait_begin");
        err = vkWaitForFences(vk_state.device, 1, &texture.upload_submit_fence,
                              VK_TRUE, UINT64_MAX);
    } else {
        err = vkGetFenceStatus(vk_state.device, texture.upload_submit_fence);
        if (err == VK_NOT_READY)
            return false;
    }
    if (err != VK_SUCCESS) {
        log_upload_stage(vk_state, texture,
                         wait_for_completion ? "wait_error" : "poll_error",
                         Strutil::fmt::format("vk_result={}",
                                              static_cast<int>(err)));
        error_message = wait_for_completion
                            ? "vkWaitForFences failed for upload async submit"
                            : "vkGetFenceStatus failed for upload async submit";
        check_vk_result(err);
        return false;
    }

    texture.upload_submit_pending = false;
    texture.source_ready          = true;
    texture.preview_dirty         = true;
    log_upload_stage(vk_state, texture,
                     wait_for_completion ? "wait_complete" : "poll_complete");
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
    destroy_texture_now(vk_state, texture);
}

void
retire_texture(VulkanState& vk_state, VulkanTexture& texture)
{
    if (!texture_has_allocated_resources(texture)
        || vk_state.device == VK_NULL_HANDLE) {
        destroy_texture_now(vk_state, texture);
        return;
    }

    RetiredVulkanTexture retired            = {};
    retired.texture                         = std::move(texture);
    retired.retire_after_main_submit_serial = vk_state.next_main_submit_serial;
    if (vk_state.verbose_logging) {
        print("imiv: Vulkan texture retire '{}' after main submit {}\n",
              retired.texture.debug_label,
              retired.retire_after_main_submit_serial);
    }
    vk_state.retired_textures.emplace_back(std::move(retired));
}

void
drain_retired_textures(VulkanState& vk_state, bool force)
{
    if (vk_state.retired_textures.empty())
        return;

    size_t write_index = 0;
    for (size_t i = 0, e = vk_state.retired_textures.size(); i < e; ++i) {
        RetiredVulkanTexture& retired = vk_state.retired_textures[i];
        const bool ready              = force
                           || vk_state.completed_main_submit_serial
                                  >= retired.retire_after_main_submit_serial;
        if (!ready) {
            if (write_index != i)
                vk_state.retired_textures[write_index] = std::move(retired);
            ++write_index;
            continue;
        }
        if (vk_state.verbose_logging) {
            print("imiv: Vulkan texture retire-drain '{}' completed={} "
                  "target={} force={}\n",
                  retired.texture.debug_label,
                  vk_state.completed_main_submit_serial,
                  retired.retire_after_main_submit_serial, force ? 1 : 0);
        }
        destroy_texture_now(vk_state, retired.texture);
    }
    vk_state.retired_textures.resize(write_index);
}

bool
create_texture(VulkanState& vk_state, const LoadedImage& image,
               VulkanTexture& texture, std::string& error_message)
{
    destroy_texture(vk_state, texture);
    texture.debug_label = image.path;

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

    size_t pixel_stride_bytes = 0;
    if (converted_pixels.empty()) {
        LoadedImageLayout image_layout;
        if (!describe_loaded_image_layout(image, image_layout, error_message)) {
            if (error_message == "invalid source row pitch")
                error_message = "invalid source stride for compute upload";
            return false;
        }
        pixel_stride_bytes = image_layout.pixel_stride_bytes;
    } else {
        pixel_stride_bytes = channel_bytes * static_cast<size_t>(channel_count);
        if (pixel_stride_bytes == 0 || row_pitch_bytes == 0
            || row_pitch_bytes
                   < static_cast<size_t>(image.width) * pixel_stride_bytes) {
            error_message = "invalid source stride for compute upload";
            return false;
        }
    }

    RowStripeUploadPlan stripe_plan;
    if (!build_row_stripe_upload_plan(
            row_pitch_bytes, pixel_stride_bytes, image.height,
            std::max<uint32_t>(1, vk_state.max_storage_buffer_range),
            std::max<uint32_t>(1, vk_state.min_storage_buffer_offset_alignment),
            stripe_plan, error_message)) {
        return false;
    }
    const VkDeviceSize upload_size_aligned = static_cast<VkDeviceSize>(
        stripe_plan.padded_upload_bytes);
    log_upload_stage(vk_state, texture, "create_begin",
                     Strutil::fmt::format(
                         "{}x{} channels={} type={} row_pitch={} stripes={} "
                         "stripe_rows={} aligned_row_pitch={} upload_bytes={} "
                         "padded_upload_bytes={}",
                         image.width, image.height, channel_count,
                         upload_data_type_name(upload_type), row_pitch_bytes,
                         stripe_plan.stripe_count, stripe_plan.stripe_rows,
                         stripe_plan.aligned_row_pitch_bytes, upload_bytes,
                         stripe_plan.padded_upload_bytes));

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

        if (!allocate_and_bind_image_memory(
                vk_state, texture.source_image,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true,
                texture.source_memory,
                "no compatible memory type for source image",
                "vkAllocateMemory failed for source image",
                "vkBindImageMemory failed for source image",
                "imiv.viewer.source_image.memory", error_message)) {
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
        if (!create_image_view_resource(
                vk_state, source_view_ci, texture.source_view,
                "vkCreateImageView failed for source image",
                "imiv.viewer.source_view", error_message)) {
            break;
        }

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

        if (!allocate_and_bind_image_memory(
                vk_state, texture.image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                true, texture.memory,
                "no compatible memory type for preview image",
                "vkAllocateMemory failed for preview image",
                "vkBindImageMemory failed for preview image",
                "imiv.viewer.preview_image.memory", error_message)) {
            break;
        }

        VkImageViewCreateInfo preview_view_ci = source_view_ci;
        preview_view_ci.image                 = texture.image;
        if (!create_image_view_resource(
                vk_state, preview_view_ci, texture.view,
                "vkCreateImageView failed for preview image",
                "imiv.viewer.preview_view", error_message)) {
            break;
        }

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

        if (!create_buffer_with_memory_resource(
                vk_state, upload_size_aligned,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT
                    | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, source_buffer,
                source_memory, "vkCreateBuffer failed for source buffer",
                "no compatible memory type for source buffer",
                "vkAllocateMemory failed for source buffer",
                "vkBindBufferMemory failed for source buffer",
                "imiv.viewer.upload.source_buffer",
                "imiv.viewer.upload.source_memory", error_message)) {
            break;
        }

        if (!create_buffer_with_memory_resource(
                vk_state, upload_size_aligned, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                    | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                false, staging_buffer, staging_memory,
                "vkCreateBuffer failed for staging buffer",
                "no host-visible memory type for staging buffer",
                "vkAllocateMemory failed for staging buffer",
                "vkBindBufferMemory failed for staging buffer",
                "imiv.viewer.upload.staging_buffer",
                "imiv.viewer.upload.staging_memory", error_message)) {
            break;
        }

        void* mapped = nullptr;
        if (!map_memory_resource(vk_state, staging_memory, upload_size_aligned,
                                 mapped,
                                 "vkMapMemory failed for staging buffer",
                                 error_message)) {
            break;
        }
        if (!copy_rows_to_padded_buffer(
                upload_ptr, upload_bytes, row_pitch_bytes, image.height,
                stripe_plan.aligned_row_pitch_bytes,
                reinterpret_cast<unsigned char*>(mapped),
                static_cast<size_t>(upload_size_aligned), error_message)) {
            vkUnmapMemory(vk_state.device, staging_memory);
            break;
        }
        vkUnmapMemory(vk_state.device, staging_memory);

        if (!allocate_descriptor_set_resource(
                vk_state, vk_state.compute_descriptor_pool,
                vk_state.compute_descriptor_set_layout, compute_set,
                "vkAllocateDescriptorSets failed for upload compute",
                error_message)) {
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
        source_buffer_info.range = stripe_plan.descriptor_range_bytes;

        VkDescriptorImageInfo output_image_info = {};
        output_image_info.imageView             = texture.source_view;
        output_image_info.imageLayout           = VK_IMAGE_LAYOUT_GENERAL;
        output_image_info.sampler               = VK_NULL_HANDLE;

        VkWriteDescriptorSet writes[2]
            = { make_buffer_descriptor_write(
                    compute_set, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                    &source_buffer_info),
                make_image_descriptor_write(compute_set, 1,
                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                            &output_image_info) };
        vkUpdateDescriptorSets(vk_state.device, 2, writes, 0, nullptr);

        VkFormatProperties output_props = {};
        vkGetPhysicalDeviceFormatProperties(vk_state.physical_device,
                                            vk_state.compute_output_format,
                                            &output_props);
        const bool has_linear_filter
            = (output_props.optimalTilingFeatures
               & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
              != 0;
        const VkFilter filter = has_linear_filter ? VK_FILTER_LINEAR
                                                  : VK_FILTER_NEAREST;
        const VkSamplerCreateInfo sampler_ci = make_clamped_sampler_create_info(
            filter, filter, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f, 1000.0f);
        if (!create_sampler_resource(vk_state, sampler_ci, texture.sampler,
                                     "vkCreateSampler failed",
                                     "imiv.viewer.sampler", error_message)) {
            break;
        }

        VkSamplerCreateInfo pixelview_sampler_ci   = sampler_ci;
        VkSamplerCreateInfo nearest_mag_sampler_ci = sampler_ci;
        nearest_mag_sampler_ci.magFilter           = VK_FILTER_NEAREST;
        pixelview_sampler_ci.magFilter             = VK_FILTER_NEAREST;
        pixelview_sampler_ci.minFilter             = VK_FILTER_NEAREST;
        pixelview_sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        if (!create_sampler_resource(
                vk_state, nearest_mag_sampler_ci, texture.nearest_mag_sampler,
                "vkCreateSampler failed for nearest-mag view",
                "imiv.viewer.nearest_mag_sampler", error_message)) {
            break;
        }
        if (!create_sampler_resource(vk_state, pixelview_sampler_ci,
                                     texture.pixelview_sampler,
                                     "vkCreateSampler failed for pixel closeup",
                                     "imiv.viewer.pixelview_sampler",
                                     error_message)) {
            break;
        }

        if (!allocate_descriptor_set_resource(
                vk_state, vk_state.preview_descriptor_pool,
                vk_state.preview_descriptor_set_layout,
                texture.preview_source_set,
                "vkAllocateDescriptorSets failed for preview source set",
                error_message)) {
            break;
        }
        VkDescriptorImageInfo preview_source_image = {};
        preview_source_image.sampler               = texture.sampler;
        preview_source_image.imageView             = texture.source_view;
        preview_source_image.imageLayout
            = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet preview_write = make_image_descriptor_write(
            texture.preview_source_set, 0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &preview_source_image);
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

        VkImageMemoryBarrier image_to_general = make_color_image_memory_barrier(
            texture.source_image, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT);

        vkCmdPipelineBarrier(upload_command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                             nullptr, 1, &source_to_compute, 1,
                             &image_to_general);

        vkCmdBindPipeline(upload_command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          use_fp64_pipeline ? vk_state.compute_pipeline_fp64
                                            : vk_state.compute_pipeline);
        for (uint32_t stripe_index = 0; stripe_index < stripe_plan.stripe_count;
             ++stripe_index) {
            const uint32_t stripe_y = stripe_index * stripe_plan.stripe_rows;
            const uint32_t remaining_rows = static_cast<uint32_t>(image.height)
                                            - stripe_y;
            const uint32_t stripe_height  = std::min(stripe_plan.stripe_rows,
                                                     remaining_rows);
            const uint32_t dynamic_offset = static_cast<uint32_t>(
                stripe_plan.descriptor_range_bytes
                * static_cast<size_t>(stripe_index));

            vkCmdBindDescriptorSets(upload_command,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    vk_state.compute_pipeline_layout, 0, 1,
                                    &compute_set, 1, &dynamic_offset);

            UploadComputePushConstants push = {};
            push.width           = static_cast<uint32_t>(image.width);
            push.height          = stripe_height;
            push.row_pitch_bytes = static_cast<uint32_t>(
                stripe_plan.aligned_row_pitch_bytes);
            push.pixel_stride  = static_cast<uint32_t>(pixel_stride_bytes);
            push.channel_count = static_cast<uint32_t>(channel_count);
            push.data_type     = static_cast<uint32_t>(upload_type);
            push.dst_x         = 0;
            push.dst_y         = stripe_y;
            vkCmdPushConstants(upload_command, vk_state.compute_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push),
                               &push);

            const uint32_t group_x = (push.width + 15u) / 16u;
            const uint32_t group_y = (push.height + 15u) / 16u;
            vkCmdDispatch(upload_command, group_x, group_y, 1);
        }

        VkImageMemoryBarrier to_shader = make_color_image_memory_barrier(
            texture.source_image, VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
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
        log_upload_stage(vk_state, texture, "submit_begin",
                         Strutil::fmt::format(
                             "command_buffer={} fence={}",
                             vk_handle_to_u64(upload_command),
                             vk_handle_to_u64(texture.upload_submit_fence)));
        err = vkQueueSubmit(vk_state.queue, 1, &submit,
                            texture.upload_submit_fence);
        if (err != VK_SUCCESS) {
            log_upload_stage(vk_state, texture, "submit_error",
                             Strutil::fmt::format("vk_result={}",
                                                  static_cast<int>(err)));
            error_message = "vkQueueSubmit failed for upload update";
            break;
        }
        log_upload_stage(vk_state, texture, "submit_complete");
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
