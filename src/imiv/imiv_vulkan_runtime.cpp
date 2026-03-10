// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_types.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

#include <imgui_impl_vulkan.h>

#include <OpenImageIO/strutil.h>

using namespace OIIO;

namespace Imiv {

#if defined(IMIV_BACKEND_VULKAN_GLFW)

namespace {

    const char* texture_status_name(ImTextureStatus status)
    {
        switch (status) {
        case ImTextureStatus_OK: return "ok";
        case ImTextureStatus_Destroyed: return "destroyed";
        case ImTextureStatus_WantCreate: return "want_create";
        case ImTextureStatus_WantUpdates: return "want_updates";
        case ImTextureStatus_WantDestroy: return "want_destroy";
        default: break;
        }
        return "unknown";
    }

    unsigned short clamp_u16(int value)
    {
        if (value <= 0)
            return 0;
        if (value
            > static_cast<int>(std::numeric_limits<unsigned short>::max())) {
            return std::numeric_limits<unsigned short>::max();
        }
        return static_cast<unsigned short>(value);
    }

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

}  // namespace

void
name_window_frame_objects(VulkanState& vk_state)
{
    if (vk_state.set_debug_object_name_fn == nullptr)
        return;

    ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
    set_vk_object_name(vk_state, VK_OBJECT_TYPE_SWAPCHAIN_KHR, wd->Swapchain,
                       "imiv.main.swapchain");
    set_vk_object_name(vk_state, VK_OBJECT_TYPE_RENDER_PASS, wd->RenderPass,
                       "imiv.main.render_pass");

    for (int i = 0; i < wd->Frames.Size; ++i) {
        char buffer_name[64] = {};
        std::snprintf(buffer_name, sizeof(buffer_name),
                      "imiv.main.frame[%d].command_buffer", i);
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_BUFFER,
                           wd->Frames[i].CommandBuffer, buffer_name);

        char image_name[64] = {};
        std::snprintf(image_name, sizeof(image_name),
                      "imiv.main.frame[%d].backbuffer", i);
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE,
                           wd->Frames[i].Backbuffer, image_name);
    }
}

void
apply_imgui_texture_update_workarounds(VulkanState& vk_state,
                                       ImDrawData* draw_data)
{
#    if defined(IMGUI_HAS_TEXTURES)
    if (draw_data == nullptr || draw_data->Textures == nullptr)
        return;

    for (ImTextureData* tex : *draw_data->Textures) {
        if (tex == nullptr || tex->Status == ImTextureStatus_OK)
            continue;

        if (vk_state.log_imgui_texture_updates) {
            print(stderr,
                  "imiv: imgui texture id={} status={} size={}x{} update=({},{} "
                  "{}x{}) pending={}\n",
                  tex->UniqueID, texture_status_name(tex->Status), tex->Width,
                  tex->Height, tex->UpdateRect.x, tex->UpdateRect.y,
                  tex->UpdateRect.w, tex->UpdateRect.h, tex->Updates.Size);
        }

        if (!vk_state.queue_requires_full_image_copies
            || tex->Status != ImTextureStatus_WantUpdates) {
            continue;
        }

        ImTextureRect full_rect = {};
        full_rect.x             = 0;
        full_rect.y             = 0;
        full_rect.w             = clamp_u16(tex->Width);
        full_rect.h             = clamp_u16(tex->Height);
        tex->UpdateRect         = full_rect;
        tex->Updates.resize(1);
        tex->Updates[0] = full_rect;

        if (!vk_state.warned_about_full_imgui_uploads) {
            print(stderr,
                  "imiv: forcing full ImGui texture uploads on this queue "
                  "family due to strict transfer granularity\n");
            vk_state.warned_about_full_imgui_uploads = true;
        }
    }
#    else
    (void)vk_state;
    (void)draw_data;
#    endif
}

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

void
frame_render(VulkanState& vk_state, ImDrawData* draw_data)
{
    ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
    if (wd->Swapchain == VK_NULL_HANDLE)
        return;

    VkResult err;
    VkSemaphore image_acquired_semaphore
        = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore
        = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    err = vkAcquireNextImageKHR(vk_state.device, wd->Swapchain, UINT64_MAX,
                                image_acquired_semaphore, VK_NULL_HANDLE,
                                &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        vk_state.swapchain_rebuild = true;
        return;
    }
    check_vk_result(err);

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(vk_state.device, 1, &fd->Fence, VK_TRUE,
                              UINT64_MAX);
        check_vk_result(err);

        err = vkResetFences(vk_state.device, 1, &fd->Fence);
        check_vk_result(err);
        err = vkResetCommandPool(vk_state.device, fd->CommandPool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        check_vk_result(err);
    }
    {
        VkRenderPassBeginInfo info   = {};
        info.sType                   = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass              = wd->RenderPass;
        info.framebuffer             = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount          = 1;
        info.pClearValues             = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info,
                             VK_SUBPASS_CONTENTS_INLINE);
    }

    apply_imgui_texture_update_workarounds(vk_state, draw_data);
    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage
            = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info         = {};
        info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount   = 1;
        info.pWaitSemaphores      = &image_acquired_semaphore;
        info.pWaitDstStageMask    = &wait_stage;
        info.commandBufferCount   = 1;
        info.pCommandBuffers      = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores    = &render_complete_semaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        check_vk_result(err);
        err = vkQueueSubmit(vk_state.queue, 1, &info, fd->Fence);
        check_vk_result(err);
    }
}

void
frame_present(VulkanState& vk_state)
{
    ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
    if (wd->Swapchain == VK_NULL_HANDLE)
        return;
    if (vk_state.swapchain_rebuild)
        return;

    VkSemaphore render_complete_semaphore
        = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info   = {};
    info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores    = &render_complete_semaphore;
    info.swapchainCount     = 1;
    info.pSwapchains        = &wd->Swapchain;
    info.pImageIndices      = &wd->FrameIndex;
    VkResult err            = vkQueuePresentKHR(vk_state.queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        vk_state.swapchain_rebuild = true;
        return;
    }
    check_vk_result(err);
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
}

bool
capture_swapchain_region_rgba8(VulkanState& vk_state, int x, int y, int w,
                               int h, unsigned int* pixels)
{
    if (pixels == nullptr || w <= 0 || h <= 0)
        return false;

    ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
    if (wd->FrameIndex >= static_cast<uint32_t>(wd->Frames.Size))
        return false;
    if (x < 0 || y < 0 || x + w > wd->Width || y + h > wd->Height)
        return false;

    VkImage image = wd->Frames[wd->FrameIndex].Backbuffer;
    if (image == VK_NULL_HANDLE)
        return false;

    const int full_width  = wd->Width;
    const int full_height = wd->Height;
    if (full_width <= 0 || full_height <= 0)
        return false;

    ImGui_ImplVulkanH_Frame* frame = &wd->Frames[wd->FrameIndex];
    VkResult err = vkWaitForFences(vk_state.device, 1, &frame->Fence, VK_TRUE,
                                   UINT64_MAX);
    if (err != VK_SUCCESS)
        return false;

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
        if (err != VK_SUCCESS)
            break;
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
            break;
        }

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize  = memory_reqs.size;
        alloc_info.memoryTypeIndex = memory_type;
        err = vkAllocateMemory(vk_state.device, &alloc_info, vk_state.allocator,
                               &staging_memory);
        if (err != VK_SUCCESS)
            break;
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY,
                           staging_memory,
                           "imiv.capture.readback.staging_memory");

        err = vkBindBufferMemory(vk_state.device, staging_buffer,
                                 staging_memory, 0);
        if (err != VK_SUCCESS)
            break;

        std::string immediate_error;
        if (!begin_immediate_submit(vk_state, command_buf, immediate_error))
            break;

        VkImageMemoryBarrier to_transfer = {};
        to_transfer.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_transfer.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        to_transfer.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        to_transfer.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        to_transfer.image                           = image;
        to_transfer.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        to_transfer.subresourceRange.baseMipLevel   = 0;
        to_transfer.subresourceRange.levelCount     = 1;
        to_transfer.subresourceRange.baseArrayLayer = 0;
        to_transfer.subresourceRange.layerCount     = 1;
        to_transfer.srcAccessMask                   = VK_ACCESS_MEMORY_READ_BIT;
        to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(command_buf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &to_transfer);

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

        VkImageMemoryBarrier to_present = {};
        to_present.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_present.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        to_present.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_present.image               = image;
        to_present.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        to_present.subresourceRange.baseMipLevel   = 0;
        to_present.subresourceRange.levelCount     = 1;
        to_present.subresourceRange.baseArrayLayer = 0;
        to_present.subresourceRange.layerCount     = 1;
        to_present.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        to_present.dstAccessMask = 0;
        vkCmdPipelineBarrier(command_buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &to_present);

        if (!end_immediate_submit(vk_state, command_buf, immediate_error))
            break;
        command_buf = VK_NULL_HANDLE;

        void* mapped = nullptr;
        err = vkMapMemory(vk_state.device, staging_memory, 0, full_buffer_size,
                          0, &mapped);
        if (err != VK_SUCCESS || mapped == nullptr)
            break;

        const unsigned char* src = static_cast<const unsigned char*>(mapped);
        for (int row = 0; row < h; ++row) {
            const int src_y         = y + row;
            const size_t src_offset = (static_cast<size_t>(src_y)
                                           * static_cast<size_t>(full_width)
                                       + static_cast<size_t>(x))
                                      * 4;
            std::memcpy(pixels
                            + static_cast<size_t>(row) * static_cast<size_t>(w),
                        src + src_offset, static_cast<size_t>(w) * 4);
        }
        vkUnmapMemory(vk_state.device, staging_memory);

        VkFormat format        = wd->SurfaceFormat.format;
        const bool bgra_source = (format == VK_FORMAT_B8G8R8A8_UNORM
                                  || format == VK_FORMAT_B8G8R8A8_SRGB);
        if (bgra_source) {
            unsigned char* bytes     = reinterpret_cast<unsigned char*>(pixels);
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
    return ok;
}

#endif

}  // namespace Imiv
