// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_vulkan_resource_utils.h"
#include "imiv_vulkan_texture_internal.h"
#include "imiv_vulkan_types.h"

#include <algorithm>
#include <string>

namespace Imiv {

#if defined(IMIV_WITH_VULKAN)

namespace {

    bool ensure_texture_preview_submit_resources(VulkanState& vk_state,
                                                 VulkanTexture& texture,
                                                 std::string& error_message)
    {
        return ensure_async_submit_resources(
            vk_state, texture.preview_command_pool,
            texture.preview_command_buffer, texture.preview_submit_fence,
            "vkCreateCommandPool failed for preview async submit",
            "vkAllocateCommandBuffers failed for preview async submit",
            "vkCreateFence failed for preview async submit",
            "imiv.preview_async.command_pool",
            "imiv.preview_async.command_buffer", "imiv.preview_async.fence",
            error_message);
    }

    bool poll_texture_preview_submission(VulkanState& vk_state,
                                         VulkanTexture& texture,
                                         const PreviewControls& controls,
                                         bool wait_for_completion,
                                         std::string& error_message)
    {
        if (!texture.preview_submit_pending)
            return true;
        if (texture.preview_submit_fence == VK_NULL_HANDLE) {
            texture.preview_submit_pending = false;
            error_message = "preview submit fence is unavailable";
            return false;
        }

        VkResult err = VK_SUCCESS;
        if (wait_for_completion) {
            err = vkWaitForFences(vk_state.device, 1,
                                  &texture.preview_submit_fence, VK_TRUE,
                                  UINT64_MAX);
        } else {
            err = vkGetFenceStatus(vk_state.device,
                                   texture.preview_submit_fence);
            if (err == VK_NOT_READY)
                return false;
        }
        if (err != VK_SUCCESS) {
            error_message
                = wait_for_completion
                      ? "vkWaitForFences failed for preview async submit"
                      : "vkGetFenceStatus failed for preview async submit";
            check_vk_result(err);
            return false;
        }

        texture.preview_submit_pending = false;
        texture.preview_initialized    = true;
        texture.preview_params_valid   = true;
        texture.last_preview_controls  = texture.preview_submit_controls;
        texture.preview_dirty
            = !preview_controls_equal(texture.last_preview_controls, controls);
        return true;
    }

}  // namespace

void
destroy_texture_preview_submit_resources(VulkanState& vk_state,
                                         VulkanTexture& texture)
{
    destroy_async_submit_resources(vk_state, texture.preview_command_pool,
                                   texture.preview_command_buffer,
                                   texture.preview_submit_fence);
    texture.preview_submit_pending = false;
}

bool
quiesce_texture_preview_submission(VulkanState& vk_state,
                                   VulkanTexture& texture,
                                   std::string& error_message)
{
    error_message.clear();
    if (!texture.preview_submit_pending)
        return true;
    return poll_texture_preview_submission(vk_state, texture,
                                           texture.preview_submit_controls,
                                           true, error_message);
}

bool
update_preview_texture(VulkanState& vk_state, VulkanTexture& texture,
                       const LoadedImage* image,
                       const PlaceholderUiState& ui_state,
                       const PreviewControls& controls,
                       std::string& error_message)
{
    if (texture.image == VK_NULL_HANDLE
        || texture.source_image == VK_NULL_HANDLE
        || texture.preview_framebuffer == VK_NULL_HANDLE
        || texture.preview_source_set == VK_NULL_HANDLE) {
        return false;
    }

    error_message.clear();
    PreviewControls effective_controls = controls;
    if (!poll_texture_upload_submission(vk_state, texture, false,
                                        error_message)) {
        if (error_message.empty())
            return true;
        return false;
    }
    if (!texture.source_ready)
        return true;

    const bool had_ocio_ready                  = vk_state.ocio.ready;
    const OcioShaderRuntime* old_ocio_runtime  = vk_state.ocio.runtime;
    const std::string old_ocio_shader_cache_id = vk_state.ocio.shader_cache_id;
    if (controls.use_ocio != 0
        && !ensure_ocio_preview_resources(vk_state, texture, image, ui_state,
                                          controls, error_message)) {
        // When OCIO is unavailable, keep the image visible by falling back to
        // the standard preview shader instead of failing the whole preview
        // update.
        if (!quiesce_texture_preview_submission(vk_state, texture,
                                                error_message)) {
            return false;
        }
        destroy_ocio_preview_resources(vk_state);
        effective_controls.use_ocio  = 0;
        texture.preview_dirty        = true;
        texture.preview_params_valid = false;
        error_message.clear();
    }
    if (effective_controls.use_ocio != 0
        && (had_ocio_ready != vk_state.ocio.ready
            || old_ocio_runtime != vk_state.ocio.runtime
            || old_ocio_shader_cache_id != vk_state.ocio.shader_cache_id)) {
        texture.preview_dirty        = true;
        texture.preview_params_valid = false;
    }

    if (!poll_texture_preview_submission(vk_state, texture, effective_controls,
                                         false, error_message)) {
        if (error_message.empty())
            return true;
        return false;
    }

    if (texture.preview_params_valid
        && preview_controls_equal(texture.last_preview_controls,
                                  effective_controls)
        && !texture.preview_dirty) {
        return true;
    }

    if (!ensure_texture_preview_submit_resources(vk_state, texture,
                                                 error_message)) {
        return false;
    }

    if (texture.preview_submit_pending)
        return true;

    bool preview_fence_signaled = false;
    if (!nonblocking_fence_status(vk_state.device, texture.preview_submit_fence,
                                  "preview async submit",
                                  preview_fence_signaled, error_message)) {
        return false;
    }
    if (!preview_fence_signaled)
        return true;

    VkResult err = VK_SUCCESS;
    err = vkResetCommandPool(vk_state.device, texture.preview_command_pool, 0);
    if (err != VK_SUCCESS) {
        error_message = "vkResetCommandPool failed for preview async submit";
        return false;
    }

    VkCommandBuffer command_buffer = texture.preview_command_buffer;

    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err         = vkBeginCommandBuffer(command_buffer, &begin);
    if (err != VK_SUCCESS) {
        error_message = "vkBeginCommandBuffer failed for preview update";
        return false;
    }

    VkImageMemoryBarrier to_color_attachment = {};
    to_color_attachment.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_color_attachment.oldLayout
        = texture.preview_initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                      : VK_IMAGE_LAYOUT_UNDEFINED;
    to_color_attachment.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    to_color_attachment.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    to_color_attachment.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    to_color_attachment.image                       = texture.image;
    to_color_attachment.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_color_attachment.subresourceRange.baseMipLevel   = 0;
    to_color_attachment.subresourceRange.levelCount     = 1;
    to_color_attachment.subresourceRange.baseArrayLayer = 0;
    to_color_attachment.subresourceRange.layerCount     = 1;
    to_color_attachment.srcAccessMask = texture.preview_initialized
                                            ? VK_ACCESS_SHADER_READ_BIT
                                            : 0;
    to_color_attachment.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(command_buffer,
                         texture.preview_initialized
                             ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                             : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &to_color_attachment);

    VkClearValue clear     = {};
    clear.color.float32[0] = 0.0f;
    clear.color.float32[1] = 0.0f;
    clear.color.float32[2] = 0.0f;
    clear.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo rp_begin   = {};
    rp_begin.sType                   = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass              = vk_state.preview_render_pass;
    rp_begin.framebuffer             = texture.preview_framebuffer;
    rp_begin.renderArea.offset       = { 0, 0 };
    rp_begin.renderArea.extent.width = static_cast<uint32_t>(texture.width);
    rp_begin.renderArea.extent.height = static_cast<uint32_t>(texture.height);
    rp_begin.clearValueCount          = 1;
    rp_begin.pClearValues             = &clear;

    vkCmdBeginRenderPass(command_buffer, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp         = {};
    vp.x                  = 0.0f;
    vp.y                  = 0.0f;
    vp.width              = static_cast<float>(texture.width);
    vp.height             = static_cast<float>(texture.height);
    vp.minDepth           = 0.0f;
    vp.maxDepth           = 1.0f;
    VkRect2D scissor      = {};
    scissor.extent.width  = static_cast<uint32_t>(texture.width);
    scissor.extent.height = static_cast<uint32_t>(texture.height);
    vkCmdSetViewport(command_buffer, 0, 1, &vp);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    const bool use_ocio_pipeline
        = effective_controls.use_ocio != 0 && vk_state.ocio.ready
          && vk_state.ocio.pipeline != VK_NULL_HANDLE
          && vk_state.ocio.pipeline_layout != VK_NULL_HANDLE
          && vk_state.ocio.descriptor_set != VK_NULL_HANDLE;
    const VkPipeline pipeline = use_ocio_pipeline ? vk_state.ocio.pipeline
                                                  : vk_state.preview_pipeline;
    const VkPipelineLayout pipeline_layout
        = use_ocio_pipeline ? vk_state.ocio.pipeline_layout
                            : vk_state.preview_pipeline_layout;
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout, 0, 1, &texture.preview_source_set,
                            0, nullptr);
    if (use_ocio_pipeline) {
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout, 1, 1,
                                &vk_state.ocio.descriptor_set, 0, nullptr);
    }

    PreviewPushConstants push = {};
    push.exposure             = effective_controls.exposure;
    push.gamma                = std::max(0.01f, effective_controls.gamma);
    push.offset               = effective_controls.offset;
    push.color_mode           = effective_controls.color_mode;
    push.channel              = effective_controls.channel;
    push.use_ocio             = effective_controls.use_ocio;
    push.orientation          = effective_controls.orientation;
    vkCmdPushConstants(command_buffer, pipeline_layout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
    vkCmdDraw(command_buffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(command_buffer);

    VkImageMemoryBarrier to_shader_read = {};
    to_shader_read.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_shader_read.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    to_shader_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader_read.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    to_shader_read.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    to_shader_read.image                           = texture.image;
    to_shader_read.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    to_shader_read.subresourceRange.baseMipLevel   = 0;
    to_shader_read.subresourceRange.levelCount     = 1;
    to_shader_read.subresourceRange.baseArrayLayer = 0;
    to_shader_read.subresourceRange.layerCount     = 1;
    to_shader_read.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    to_shader_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &to_shader_read);

    err = vkEndCommandBuffer(command_buffer);
    if (err != VK_SUCCESS) {
        error_message = "vkEndCommandBuffer failed for preview update";
        return false;
    }
    err = vkResetFences(vk_state.device, 1, &texture.preview_submit_fence);
    if (err != VK_SUCCESS) {
        error_message = "vkResetFences failed for preview async submit";
        return false;
    }

    VkSubmitInfo submit       = {};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &command_buffer;
    err                       = vkQueueSubmit(vk_state.queue, 1, &submit,
                                              texture.preview_submit_fence);
    if (err != VK_SUCCESS) {
        error_message = "vkQueueSubmit failed for preview update";
        return false;
    }

    texture.preview_submit_pending  = true;
    texture.preview_submit_controls = effective_controls;
    return true;
}

#endif

}  // namespace Imiv
