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

#if defined(IMIV_WITH_VULKAN)

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
frame_render(VulkanState& vk_state, ImDrawData* draw_data)
{
    ImGui_ImplVulkanH_Window* wd = &vk_state.window_data;
    if (wd->Swapchain == VK_NULL_HANDLE)
        return;

    VkResult err;
    const uint32_t semaphore_index = wd->SemaphoreIndex;
    VkSemaphore image_acquired_semaphore
        = wd->FrameSemaphores[semaphore_index].ImageAcquiredSemaphore;
    err = vkAcquireNextImageKHR(vk_state.device, wd->Swapchain, UINT64_MAX,
                                image_acquired_semaphore, VK_NULL_HANDLE,
                                &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        vk_state.swapchain_rebuild = true;
        return;
    }
    check_vk_result(err);

    VkSemaphore render_complete_semaphore
        = wd->FrameSemaphores[semaphore_index].RenderCompleteSemaphore;

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

#endif

}  // namespace Imiv
