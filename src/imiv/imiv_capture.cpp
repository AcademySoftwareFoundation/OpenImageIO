// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_types.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace Imiv {

#if defined(IMIV_WITH_VULKAN)

namespace {

    bool ensure_immediate_submit_resources(VulkanState& vk_state,
                                           std::string& error_message)
    {
        if (vk_state.immediate_command_pool != VK_NULL_HANDLE
            && vk_state.immediate_command_buffer != VK_NULL_HANDLE
            && vk_state.immediate_submit_fence != VK_NULL_HANDLE) {
            return true;
        }

        destroy_immediate_submit_resources(vk_state);

        VkResult err                    = VK_SUCCESS;
        VkCommandPoolCreateInfo pool_ci = {};
        pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
                        | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_ci.queueFamilyIndex = vk_state.queue_family;
        err = vkCreateCommandPool(vk_state.device, &pool_ci, vk_state.allocator,
                                  &vk_state.immediate_command_pool);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateCommandPool failed for immediate submit";
            destroy_immediate_submit_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_POOL,
                           vk_state.immediate_command_pool,
                           "imiv.immediate_submit.command_pool");

        VkCommandBufferAllocateInfo command_alloc = {};
        command_alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_alloc.commandPool        = vk_state.immediate_command_pool;
        command_alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_alloc.commandBufferCount = 1;
        err = vkAllocateCommandBuffers(vk_state.device, &command_alloc,
                                       &vk_state.immediate_command_buffer);
        if (err != VK_SUCCESS) {
            error_message
                = "vkAllocateCommandBuffers failed for immediate submit";
            destroy_immediate_submit_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_BUFFER,
                           vk_state.immediate_command_buffer,
                           "imiv.immediate_submit.command_buffer");

        VkFenceCreateInfo fence_ci = {};
        fence_ci.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_ci.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
        err = vkCreateFence(vk_state.device, &fence_ci, vk_state.allocator,
                            &vk_state.immediate_submit_fence);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateFence failed for immediate submit";
            destroy_immediate_submit_resources(vk_state);
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_FENCE,
                           vk_state.immediate_submit_fence,
                           "imiv.immediate_submit.fence");
        return true;
    }

    bool capture_swapchain_region_rgba8_from_layout(
        VulkanState& vk_state, int x, int y, int w, int h,
        unsigned int* pixels, VkImageLayout source_layout,
        const char* source_layout_name, std::string& error_message)
    {
        if (pixels == nullptr || w <= 0 || h <= 0) {
            error_message = "invalid Vulkan capture buffer or size";
            return false;
        }

        ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
        if (wd->FrameIndex >= static_cast<uint32_t>(wd->Frames.Size)) {
            error_message = "invalid Vulkan swapchain frame index";
            return false;
        }
        if (x < 0 || y < 0 || x + w > wd->Width || y + h > wd->Height) {
            error_message = "capture rectangle is outside the Vulkan swapchain image";
            return false;
        }

        VkImage image = wd->Frames[wd->FrameIndex].Backbuffer;
        if (image == VK_NULL_HANDLE) {
            error_message = "Vulkan swapchain backbuffer is null";
            return false;
        }

        const int full_width  = wd->Width;
        const int full_height = wd->Height;
        if (full_width <= 0 || full_height <= 0) {
            error_message = "Vulkan swapchain image size is invalid";
            return false;
        }

        VkResult err = vkQueueWaitIdle(vk_state.queue);
        if (err != VK_SUCCESS) {
            error_message = "vkQueueWaitIdle failed before Vulkan screen capture";
            return false;
        }

        VkBuffer staging_buffer       = VK_NULL_HANDLE;
        VkDeviceMemory staging_memory = VK_NULL_HANDLE;
        VkCommandBuffer command_buf   = VK_NULL_HANDLE;
        const VkDeviceSize full_buffer_size
            = static_cast<VkDeviceSize>(full_width)
              * static_cast<VkDeviceSize>(full_height) * 4;
        bool ok = false;

        do {
            VkBufferCreateInfo buffer_ci = {};
            buffer_ci.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_ci.size               = full_buffer_size;
            buffer_ci.usage              = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            buffer_ci.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
            err = vkCreateBuffer(vk_state.device, &buffer_ci, vk_state.allocator,
                                 &staging_buffer);
            if (err != VK_SUCCESS) {
                error_message
                    = "vkCreateBuffer failed for Vulkan capture staging buffer";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_BUFFER, staging_buffer,
                               "imiv.capture.readback.staging_buffer");

            VkMemoryRequirements memory_reqs = {};
            vkGetBufferMemoryRequirements(vk_state.device, staging_buffer,
                                          &memory_reqs);

            uint32_t memory_type = 0;
            if (!find_memory_type(vk_state.physical_device,
                                  memory_reqs.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  memory_type)) {
                error_message
                    = "failed to find Vulkan host-visible staging memory type";
                break;
            }

            VkMemoryAllocateInfo alloc_info = {};
            alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize  = memory_reqs.size;
            alloc_info.memoryTypeIndex = memory_type;
            err = vkAllocateMemory(vk_state.device, &alloc_info,
                                   vk_state.allocator, &staging_memory);
            if (err != VK_SUCCESS) {
                error_message
                    = "vkAllocateMemory failed for Vulkan capture staging buffer";
                break;
            }
            set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                               staging_memory,
                               "imiv.capture.readback.staging_memory");

            err = vkBindBufferMemory(vk_state.device, staging_buffer,
                                     staging_memory, 0);
            if (err != VK_SUCCESS) {
                error_message = "vkBindBufferMemory failed for Vulkan capture";
                break;
            }

            std::string immediate_error;
            if (!begin_immediate_submit(vk_state, command_buf, immediate_error)) {
                error_message = immediate_error;
                break;
            }

            VkImageMemoryBarrier to_transfer = {};
            to_transfer.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            to_transfer.oldLayout = source_layout;
            to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            to_transfer.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            to_transfer.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            to_transfer.image                           = image;
            to_transfer.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            to_transfer.subresourceRange.baseMipLevel   = 0;
            to_transfer.subresourceRange.levelCount     = 1;
            to_transfer.subresourceRange.baseArrayLayer = 0;
            to_transfer.subresourceRange.layerCount     = 1;
            to_transfer.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT
                                        | VK_ACCESS_MEMORY_WRITE_BIT;
            to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(command_buf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                                 0, nullptr, 1, &to_transfer);

            VkBufferImageCopy copy_region               = {};
            copy_region.bufferOffset                    = 0;
            copy_region.bufferRowLength                 = 0;
            copy_region.bufferImageHeight               = 0;
            copy_region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.imageSubresource.mipLevel       = 0;
            copy_region.imageSubresource.baseArrayLayer = 0;
            copy_region.imageSubresource.layerCount     = 1;
            copy_region.imageOffset.x                   = 0;
            copy_region.imageOffset.y                   = 0;
            copy_region.imageOffset.z                   = 0;
            copy_region.imageExtent.width  = static_cast<uint32_t>(full_width);
            copy_region.imageExtent.height = static_cast<uint32_t>(full_height);
            copy_region.imageExtent.depth  = 1;
            vkCmdCopyImageToBuffer(command_buf, image,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   staging_buffer, 1, &copy_region);

            VkImageMemoryBarrier restore_layout = {};
            restore_layout.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            restore_layout.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            restore_layout.newLayout           = source_layout;
            restore_layout.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            restore_layout.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            restore_layout.image               = image;
            restore_layout.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            restore_layout.subresourceRange.baseMipLevel = 0;
            restore_layout.subresourceRange.levelCount   = 1;
            restore_layout.subresourceRange.baseArrayLayer = 0;
            restore_layout.subresourceRange.layerCount     = 1;
            restore_layout.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            restore_layout.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            vkCmdPipelineBarrier(command_buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &restore_layout);

            if (!end_immediate_submit(vk_state, command_buf, immediate_error)) {
                error_message = immediate_error;
                break;
            }
            command_buf = VK_NULL_HANDLE;

            void* mapped = nullptr;
            err = vkMapMemory(vk_state.device, staging_memory, 0,
                              full_buffer_size, 0, &mapped);
            if (err != VK_SUCCESS || mapped == nullptr) {
                error_message = "vkMapMemory failed for Vulkan capture readback";
                break;
            }

            const unsigned char* src = static_cast<const unsigned char*>(mapped);
            for (int row = 0; row < h; ++row) {
                const int src_y         = y + row;
                const size_t src_offset = (static_cast<size_t>(src_y)
                                               * static_cast<size_t>(full_width)
                                           + static_cast<size_t>(x))
                                          * 4;
                std::memcpy(
                    pixels + static_cast<size_t>(row) * static_cast<size_t>(w),
                    src + src_offset, static_cast<size_t>(w) * 4);
            }
            vkUnmapMemory(vk_state.device, staging_memory);

            const VkFormat format = wd->SurfaceFormat.format;
            const bool bgra_source = (format == VK_FORMAT_B8G8R8A8_UNORM
                                      || format == VK_FORMAT_B8G8R8A8_SRGB);
            if (bgra_source) {
                unsigned char* bytes = reinterpret_cast<unsigned char*>(pixels);
                const size_t pixel_count = static_cast<size_t>(w)
                                           * static_cast<size_t>(h);
                for (size_t i = 0; i < pixel_count; ++i) {
                    unsigned char* px = bytes + i * 4;
                    std::swap(px[0], px[2]);
                }
            }
            ok = true;
        } while (false);

        if (staging_buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(vk_state.device, staging_buffer, vk_state.allocator);
        if (staging_memory != VK_NULL_HANDLE)
            vkFreeMemory(vk_state.device, staging_memory, vk_state.allocator);

        if (!ok && error_message.empty()) {
            error_message
                = std::string("unknown Vulkan screen capture failure from ")
                  + source_layout_name;
        }
        return ok;
    }

}  // namespace

void
destroy_immediate_submit_resources(VulkanState& vk_state)
{
    if (vk_state.immediate_submit_fence != VK_NULL_HANDLE) {
        vkDestroyFence(vk_state.device, vk_state.immediate_submit_fence,
                       vk_state.allocator);
        vk_state.immediate_submit_fence = VK_NULL_HANDLE;
    }
    vk_state.immediate_command_buffer = VK_NULL_HANDLE;
    if (vk_state.immediate_command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vk_state.device, vk_state.immediate_command_pool,
                             vk_state.allocator);
        vk_state.immediate_command_pool = VK_NULL_HANDLE;
    }
}

bool
begin_immediate_submit(VulkanState& vk_state, VkCommandBuffer& out_command,
                       std::string& error_message)
{
    out_command = VK_NULL_HANDLE;
    if (!ensure_immediate_submit_resources(vk_state, error_message))
        return false;

    VkResult err = vkWaitForFences(vk_state.device, 1,
                                   &vk_state.immediate_submit_fence, VK_TRUE,
                                   UINT64_MAX);
    if (err != VK_SUCCESS) {
        error_message = "vkWaitForFences failed for immediate submit";
        return false;
    }
    err = vkResetCommandPool(vk_state.device, vk_state.immediate_command_pool,
                             0);
    if (err != VK_SUCCESS) {
        error_message = "vkResetCommandPool failed for immediate submit";
        return false;
    }

    out_command                         = vk_state.immediate_command_buffer;
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err              = vkBeginCommandBuffer(out_command, &begin_info);
    if (err != VK_SUCCESS) {
        error_message = "vkBeginCommandBuffer failed for immediate submit";
        out_command   = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

bool
end_immediate_submit(VulkanState& vk_state, VkCommandBuffer command_buffer,
                     std::string& error_message)
{
    if (command_buffer == VK_NULL_HANDLE
        || command_buffer != vk_state.immediate_command_buffer) {
        error_message = "invalid immediate-submit command buffer";
        return false;
    }

    VkResult err = vkEndCommandBuffer(command_buffer);
    if (err != VK_SUCCESS) {
        error_message = "vkEndCommandBuffer failed for immediate submit";
        return false;
    }

    err = vkResetFences(vk_state.device, 1, &vk_state.immediate_submit_fence);
    if (err != VK_SUCCESS) {
        error_message = "vkResetFences failed for immediate submit";
        return false;
    }

    VkSubmitInfo submit_info       = {};
    submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers    = &command_buffer;
    err = vkQueueSubmit(vk_state.queue, 1, &submit_info,
                        vk_state.immediate_submit_fence);
    if (err != VK_SUCCESS) {
        error_message = "vkQueueSubmit failed for immediate submit";
        destroy_immediate_submit_resources(vk_state);
        return false;
    }
    err = vkWaitForFences(vk_state.device, 1, &vk_state.immediate_submit_fence,
                          VK_TRUE, UINT64_MAX);
    if (err != VK_SUCCESS) {
        error_message = "vkWaitForFences failed after immediate submit";
        return false;
    }
    return true;
}

bool
capture_swapchain_region_rgba8(VulkanState& vk_state, int x, int y, int w,
                               int h, unsigned int* pixels)
{
    std::string error_message;
    if (capture_swapchain_region_rgba8_from_layout(
            vk_state, x, y, w, h, pixels, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            "VK_IMAGE_LAYOUT_PRESENT_SRC_KHR", error_message)) {
        return true;
    }

    if (capture_swapchain_region_rgba8_from_layout(
            vk_state, x, y, w, h, pixels, VK_IMAGE_LAYOUT_GENERAL,
            "VK_IMAGE_LAYOUT_GENERAL", error_message)) {
        return true;
    }

    std::fprintf(stderr, "imiv: Vulkan screen capture failed: %s\n",
                 error_message.empty() ? "unknown error"
                                       : error_message.c_str());
    return false;
}

bool
imiv_vulkan_screen_capture(ImGuiID viewport_id, int x, int y, int w, int h,
                           unsigned int* pixels, void* user_data)
{
    VulkanState* vk_state = reinterpret_cast<VulkanState*>(user_data);
    if (vk_state == nullptr)
        return false;

    int capture_x = x;
    int capture_y = y;
    int capture_w = w;
    int capture_h = h;
    ImGuiViewport* viewport = ImGui::FindViewportByID(viewport_id);
    if (viewport != nullptr && vk_state->window_data.Width > 0
        && vk_state->window_data.Height > 0 && viewport->Size.x > 0.0f
        && viewport->Size.y > 0.0f) {
        const double scale_x = static_cast<double>(vk_state->window_data.Width)
                               / static_cast<double>(viewport->Size.x);
        const double scale_y = static_cast<double>(vk_state->window_data.Height)
                               / static_cast<double>(viewport->Size.y);
        capture_x = static_cast<int>(std::lround(
            (static_cast<double>(x) - static_cast<double>(viewport->Pos.x))
            * scale_x));
        capture_y = static_cast<int>(std::lround(
            (static_cast<double>(y) - static_cast<double>(viewport->Pos.y))
            * scale_y));
        capture_w = std::max(1, static_cast<int>(std::lround(
                                   static_cast<double>(w) * scale_x)));
        capture_h = std::max(1, static_cast<int>(std::lround(
                                   static_cast<double>(h) * scale_y)));
    }

    if (capture_x < 0) {
        capture_w += capture_x;
        capture_x = 0;
    }
    if (capture_y < 0) {
        capture_h += capture_y;
        capture_y = 0;
    }
    if (capture_x < vk_state->window_data.Width
        && capture_y < vk_state->window_data.Height) {
        capture_w = std::min(capture_w, vk_state->window_data.Width - capture_x);
        capture_h = std::min(capture_h,
                             vk_state->window_data.Height - capture_y);
    }

    return capture_swapchain_region_rgba8(*vk_state, capture_x, capture_y,
                                          capture_w, capture_h, pixels);
}

#endif

}  // namespace Imiv
