// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_ocio.h"
#include "imiv_vulkan_resource_utils.h"
#include "imiv_vulkan_shader_utils.h"
#include "imiv_vulkan_types.h"

#include <OpenImageIO/half.h>
#include <OpenImageIO/strutil.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#if defined(IMIV_WITH_VULKAN) && defined(IMIV_HAS_EMBEDDED_VULKAN_SHADERS) \
    && IMIV_HAS_EMBEDDED_VULKAN_SHADERS
#    include "imiv_preview_vert_spv.h"
#endif

namespace Imiv {

#if defined(IMIV_WITH_VULKAN)

namespace {

    void destroy_ocio_texture(VulkanState& vk_state, OcioVulkanTexture& texture)
    {
        if (texture.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(vk_state.device, texture.sampler,
                             vk_state.allocator);
            texture.sampler = VK_NULL_HANDLE;
        }
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
    }

    VkFormat select_ocio_format(const OcioTextureBlueprint& texture)
    {
        if (texture.channel == OcioTextureChannel::Red)
            return VK_FORMAT_R16_SFLOAT;
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    }

    bool check_ocio_format_features(VulkanState& vk_state, VkFormat format,
                                    bool require_linear_filter,
                                    std::string& error_message)
    {
        VkFormatProperties props = {};
        vkGetPhysicalDeviceFormatProperties(vk_state.physical_device, format,
                                            &props);
        const VkFormatFeatureFlags features = props.optimalTilingFeatures;
        if ((features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0) {
            error_message = OIIO::Strutil::fmt::format(
                "OCIO texture format {} is not sampleable",
                static_cast<int>(format));
            return false;
        }
        if (require_linear_filter
            && (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
                   == 0) {
            error_message = OIIO::Strutil::fmt::format(
                "OCIO texture format {} does not support linear filtering",
                static_cast<int>(format));
            return false;
        }
        return true;
    }

    bool create_ocio_sampler(VulkanState& vk_state,
                             const OcioTextureBlueprint& blueprint,
                             VkSampler& sampler, std::string& error_message)
    {
        const VkFilter filter
            = blueprint.interpolation == OcioInterpolation::Nearest
                  ? VK_FILTER_NEAREST
                  : VK_FILTER_LINEAR;
        const VkSamplerCreateInfo create_info = make_clamped_sampler_create_info(
            filter, filter,
            filter == VK_FILTER_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST
                                        : VK_SAMPLER_MIPMAP_MODE_LINEAR,
            0.0f, 0.0f);
        return create_sampler_resource(
            vk_state, create_info, sampler,
            "vkCreateSampler failed for OCIO texture",
            "imiv.ocio.texture.sampler", error_message);
    }

    void build_ocio_upload_data(const OcioTextureBlueprint& blueprint,
                                VkFormat format,
                                std::vector<unsigned char>& upload_bytes)
    {
        upload_bytes.clear();
        const size_t texel_count = static_cast<size_t>(blueprint.width)
                                   * static_cast<size_t>(blueprint.height)
                                   * static_cast<size_t>(
                                       std::max(1u, blueprint.depth));
        if (format == VK_FORMAT_R16_SFLOAT) {
            upload_bytes.resize(texel_count * sizeof(uint16_t));
            auto* dst = reinterpret_cast<uint16_t*>(upload_bytes.data());
            for (size_t i = 0; i < texel_count; ++i)
                dst[i] = half(blueprint.values[i]).bits();
            return;
        }

        upload_bytes.resize(texel_count * 4u * sizeof(uint16_t));
        auto* dst = reinterpret_cast<uint16_t*>(upload_bytes.data());
        for (size_t i = 0; i < texel_count; ++i) {
            const size_t src     = i * 3u;
            const size_t dst_idx = i * 4u;
            dst[dst_idx + 0]     = half(blueprint.values[src + 0]).bits();
            dst[dst_idx + 1]     = half(blueprint.values[src + 1]).bits();
            dst[dst_idx + 2]     = half(blueprint.values[src + 2]).bits();
            dst[dst_idx + 3]     = half(1.0f).bits();
        }
    }

    bool upload_ocio_texture(VulkanState& vk_state,
                             const OcioTextureBlueprint& blueprint,
                             OcioVulkanTexture& texture,
                             std::string& error_message)
    {
        texture         = {};
        texture.binding = blueprint.shader_binding;
        texture.width   = static_cast<int>(blueprint.width);
        texture.height  = static_cast<int>(blueprint.height);
        texture.depth   = static_cast<int>(blueprint.depth);

        const VkFormat format = select_ocio_format(blueprint);
        if (!check_ocio_format_features(vk_state, format,
                                        blueprint.interpolation
                                            != OcioInterpolation::Nearest,
                                        error_message)) {
            return false;
        }

        std::vector<unsigned char> upload_bytes;
        build_ocio_upload_data(blueprint, format, upload_bytes);

        VkImageCreateInfo image_ci = {};
        image_ci.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_ci.imageType         = blueprint.dimensions
                                     == OcioTextureDimensions::Tex3D
                                         ? VK_IMAGE_TYPE_3D
                                         : VK_IMAGE_TYPE_2D;
        image_ci.format            = format;
        image_ci.extent.width      = blueprint.width;
        image_ci.extent.height     = std::max(1u, blueprint.height);
        image_ci.extent.depth      = std::max(1u, blueprint.depth);
        image_ci.mipLevels         = 1;
        image_ci.arrayLayers       = 1;
        image_ci.samples           = VK_SAMPLE_COUNT_1_BIT;
        image_ci.tiling            = VK_IMAGE_TILING_OPTIMAL;
        image_ci.usage             = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                         | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_ci.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
        const VkResult image_err = vkCreateImage(vk_state.device, &image_ci,
                                                 vk_state.allocator,
                                                 &texture.image);
        if (image_err != VK_SUCCESS) {
            error_message = "vkCreateImage failed for OCIO texture";
            return false;
        }

        if (!allocate_and_bind_image_memory(
                vk_state, texture.image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                true, texture.memory,
                "failed to find memory type for OCIO texture",
                "vkAllocateMemory failed for OCIO texture",
                "vkBindImageMemory failed for OCIO texture", nullptr,
                error_message)) {
            return false;
        }

        VkBuffer staging_buffer       = VK_NULL_HANDLE;
        VkDeviceMemory staging_memory = VK_NULL_HANDLE;
        VkBufferCreateInfo buffer_ci  = {};
        buffer_ci.sType               = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_ci.size                = upload_bytes.size();
        buffer_ci.usage               = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_ci.sharingMode         = VK_SHARING_MODE_EXCLUSIVE;
        const VkResult buffer_err = vkCreateBuffer(vk_state.device, &buffer_ci,
                                                   vk_state.allocator,
                                                   &staging_buffer);
        if (buffer_err != VK_SUCCESS) {
            error_message = "vkCreateBuffer failed for OCIO staging texture";
            return false;
        }

        if (!allocate_and_bind_buffer_memory(
                vk_state, staging_buffer,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                    | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                false, staging_memory,
                "failed to find host-visible memory for OCIO texture",
                "vkAllocateMemory failed for OCIO staging texture",
                "vkBindBufferMemory failed for OCIO staging texture", nullptr,
                error_message)) {
            vkDestroyBuffer(vk_state.device, staging_buffer,
                            vk_state.allocator);
            return false;
        }

        void* mapped = nullptr;
        if (!map_memory_resource(vk_state, staging_memory, upload_bytes.size(),
                                 mapped,
                                 "vkMapMemory failed for OCIO staging texture",
                                 error_message)) {
            vkFreeMemory(vk_state.device, staging_memory, vk_state.allocator);
            vkDestroyBuffer(vk_state.device, staging_buffer,
                            vk_state.allocator);
            return false;
        }
        std::memcpy(mapped, upload_bytes.data(), upload_bytes.size());
        vkUnmapMemory(vk_state.device, staging_memory);

        VkImageAspectFlags aspect      = VK_IMAGE_ASPECT_COLOR_BIT;
        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        if (!begin_immediate_submit(vk_state, command_buffer, error_message)) {
            vkFreeMemory(vk_state.device, staging_memory, vk_state.allocator);
            vkDestroyBuffer(vk_state.device, staging_buffer,
                            vk_state.allocator);
            return false;
        }

        VkImageMemoryBarrier to_transfer = {};
        to_transfer.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_transfer.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        to_transfer.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        to_transfer.image                           = texture.image;
        to_transfer.subresourceRange.aspectMask     = aspect;
        to_transfer.subresourceRange.baseMipLevel   = 0;
        to_transfer.subresourceRange.levelCount     = 1;
        to_transfer.subresourceRange.baseArrayLayer = 0;
        to_transfer.subresourceRange.layerCount     = 1;
        to_transfer.srcAccessMask                   = 0;
        to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &to_transfer);

        VkBufferImageCopy region               = {};
        region.imageSubresource.aspectMask     = aspect;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;
        region.imageExtent.width               = blueprint.width;
        region.imageExtent.height              = std::max(1u, blueprint.height);
        region.imageExtent.depth               = std::max(1u, blueprint.depth);
        vkCmdCopyBufferToImage(command_buffer, staging_buffer, texture.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &region);

        VkImageMemoryBarrier to_sampled = {};
        to_sampled.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_sampled.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_sampled.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        to_sampled.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        to_sampled.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        to_sampled.image                           = texture.image;
        to_sampled.subresourceRange.aspectMask     = aspect;
        to_sampled.subresourceRange.baseMipLevel   = 0;
        to_sampled.subresourceRange.levelCount     = 1;
        to_sampled.subresourceRange.baseArrayLayer = 0;
        to_sampled.subresourceRange.layerCount     = 1;
        to_sampled.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_sampled.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &to_sampled);

        if (!end_immediate_submit(vk_state, command_buffer, error_message)) {
            vkFreeMemory(vk_state.device, staging_memory, vk_state.allocator);
            vkDestroyBuffer(vk_state.device, staging_buffer,
                            vk_state.allocator);
            return false;
        }

        vkFreeMemory(vk_state.device, staging_memory, vk_state.allocator);
        vkDestroyBuffer(vk_state.device, staging_buffer, vk_state.allocator);

        VkImageViewCreateInfo view_ci = {};
        view_ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_ci.image    = texture.image;
        view_ci.viewType = blueprint.dimensions == OcioTextureDimensions::Tex3D
                               ? VK_IMAGE_VIEW_TYPE_3D
                               : VK_IMAGE_VIEW_TYPE_2D;
        view_ci.format   = format;
        view_ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        view_ci.subresourceRange.baseMipLevel   = 0;
        view_ci.subresourceRange.levelCount     = 1;
        view_ci.subresourceRange.baseArrayLayer = 0;
        view_ci.subresourceRange.layerCount     = 1;
        if (!create_image_view_resource(
                vk_state, view_ci, texture.view,
                "vkCreateImageView failed for OCIO texture", nullptr,
                error_message)) {
            return false;
        }

        return create_ocio_sampler(vk_state, blueprint, texture.sampler,
                                   error_message);
    }

    bool create_ocio_uniform_buffer_resource(VulkanState& vk_state, size_t size,
                                             std::string& error_message)
    {
        if (size == 0)
            return true;

        VkBufferCreateInfo buffer_ci = {};
        buffer_ci.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_ci.size               = size;
        buffer_ci.usage              = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        buffer_ci.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
        const VkResult buffer_err
            = vkCreateBuffer(vk_state.device, &buffer_ci, vk_state.allocator,
                             &vk_state.ocio.uniform_buffer);
        if (buffer_err != VK_SUCCESS) {
            error_message = "vkCreateBuffer failed for OCIO uniform buffer";
            return false;
        }

        if (!allocate_and_bind_buffer_memory(
                vk_state, vk_state.ocio.uniform_buffer,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                    | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                false, vk_state.ocio.uniform_memory,
                "failed to find memory type for OCIO uniform buffer",
                "vkAllocateMemory failed for OCIO uniform buffer",
                "vkBindBufferMemory failed for OCIO uniform buffer", nullptr,
                error_message)) {
            return false;
        }
        if (!map_memory_resource(vk_state, vk_state.ocio.uniform_memory, size,
                                 vk_state.ocio.uniform_mapped,
                                 "vkMapMemory failed for OCIO uniform buffer",
                                 error_message)) {
            return false;
        }
        vk_state.ocio.uniform_buffer_size = size;
        return true;
    }

    bool update_ocio_uniform_buffer_resource(VulkanState& vk_state,
                                             const PreviewControls& controls,
                                             std::string& error_message)
    {
        if (vk_state.ocio.runtime == nullptr)
            return true;
        std::vector<unsigned char> uniform_bytes;
        if (!build_ocio_uniform_buffer(*vk_state.ocio.runtime, controls,
                                       uniform_bytes, error_message)) {
            return false;
        }
        if (uniform_bytes.empty())
            return true;
        if (vk_state.ocio.uniform_mapped == nullptr
            || uniform_bytes.size() > vk_state.ocio.uniform_buffer_size) {
            error_message = "OCIO uniform buffer is unavailable";
            return false;
        }
        std::memcpy(vk_state.ocio.uniform_mapped, uniform_bytes.data(),
                    uniform_bytes.size());
        return true;
    }

    bool create_ocio_descriptor_resources(VulkanState& vk_state,
                                          std::string& error_message)
    {
        const OcioShaderBlueprint& blueprint = vk_state.ocio.runtime->blueprint;

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(1 + blueprint.textures.size());

        bindings.push_back(make_descriptor_set_layout_binding(
            0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT));

        for (const OcioTextureBlueprint& texture : blueprint.textures) {
            bindings.push_back(make_descriptor_set_layout_binding(
                texture.shader_binding,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT));
        }

        const VkDescriptorPoolSize pool_sizes[]
            = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                  static_cast<uint32_t>(blueprint.textures.size()) } };
        const uint32_t pool_size_count = blueprint.textures.empty() ? 1u : 2u;
        if (!create_descriptor_pool_resource(
                vk_state, 0, 1, pool_sizes, pool_size_count,
                vk_state.ocio.descriptor_pool,
                "vkCreateDescriptorPool failed for OCIO",
                "imiv.ocio.descriptor_pool", error_message)) {
            return false;
        }

        if (!create_descriptor_set_layout_resource(
                vk_state, bindings.data(),
                static_cast<uint32_t>(bindings.size()),
                vk_state.ocio.descriptor_set_layout,
                "vkCreateDescriptorSetLayout failed for OCIO",
                "imiv.ocio.set_layout", error_message)) {
            return false;
        }

        if (!allocate_descriptor_set_resource(
                vk_state, vk_state.ocio.descriptor_pool,
                vk_state.ocio.descriptor_set_layout,
                vk_state.ocio.descriptor_set,
                "vkAllocateDescriptorSets failed for OCIO", error_message)) {
            return false;
        }

        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorImageInfo> image_infos;
        writes.reserve(1 + blueprint.textures.size());
        image_infos.reserve(blueprint.textures.size());

        VkDescriptorBufferInfo buffer_info = {};
        buffer_info.buffer                 = vk_state.ocio.uniform_buffer;
        buffer_info.offset                 = 0;
        buffer_info.range                  = blueprint.uniform_buffer_size;

        writes.push_back(
            make_buffer_descriptor_write(vk_state.ocio.descriptor_set, 0,
                                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                         &buffer_info));

        for (const OcioVulkanTexture& texture : vk_state.ocio.textures) {
            VkDescriptorImageInfo info = {};
            info.sampler               = texture.sampler;
            info.imageView             = texture.view;
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_infos.push_back(info);
        }
        for (size_t i = 0; i < vk_state.ocio.textures.size(); ++i) {
            writes.push_back(make_image_descriptor_write(
                vk_state.ocio.descriptor_set, vk_state.ocio.textures[i].binding,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &image_infos[i]));
        }
        vkUpdateDescriptorSets(vk_state.device,
                               static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
        return true;
    }

    bool create_ocio_pipeline(VulkanState& vk_state, std::string& error_message)
    {
        VkShaderModule vert_module = VK_NULL_HANDLE;
        VkShaderModule frag_module = VK_NULL_HANDLE;
        std::vector<uint32_t> frag_words;
        if (!compile_ocio_preview_fragment_spirv(
                vk_state.ocio.runtime->blueprint, frag_words, error_message)) {
            return false;
        }

        const std::string shader_vert = std::string(IMIV_SHADER_DIR)
                                        + "/imiv_preview.vert.spv";
#    if defined(IMIV_HAS_EMBEDDED_VULKAN_SHADERS) \
        && IMIV_HAS_EMBEDDED_VULKAN_SHADERS
        const uint32_t* shader_vert_words = g_imiv_preview_vert_spv;
        const size_t shader_vert_word_count = g_imiv_preview_vert_spv_word_count;
#    else
        const uint32_t* shader_vert_words   = nullptr;
        const size_t shader_vert_word_count = 0;
#    endif
        if (!create_shader_module_from_embedded_or_file(
                vk_state.device, vk_state.allocator, shader_vert_words,
                shader_vert_word_count, shader_vert, "imiv.preview.vert",
                vert_module, error_message)) {
            return false;
        }
        if (!create_shader_module_from_words(
                vk_state.device, vk_state.allocator, frag_words.data(),
                frag_words.size(), "imiv.ocio.preview.frag", frag_module,
                error_message)) {
            vkDestroyShaderModule(vk_state.device, vert_module,
                                  vk_state.allocator);
            return false;
        }

        VkDescriptorSetLayout set_layouts[2] = {
            vk_state.preview_descriptor_set_layout,
            vk_state.ocio.descriptor_set_layout,
        };
        VkPushConstantRange push = {};
        push.stageFlags          = VK_SHADER_STAGE_FRAGMENT_BIT;
        push.offset              = 0;
        push.size                = sizeof(PreviewPushConstants);

        if (!create_pipeline_layout_resource(
                vk_state, set_layouts,
                static_cast<uint32_t>(IM_ARRAYSIZE(set_layouts)), &push, 1,
                vk_state.ocio.pipeline_layout,
                "vkCreatePipelineLayout failed for OCIO preview",
                "imiv.ocio.pipeline_layout", error_message)) {
            vkDestroyShaderModule(vk_state.device, frag_module,
                                  vk_state.allocator);
            vkDestroyShaderModule(vk_state.device, vert_module,
                                  vk_state.allocator);
            return false;
        }

        const bool pipeline_ok = create_fullscreen_preview_pipeline(
            vk_state, vk_state.preview_render_pass,
            vk_state.ocio.pipeline_layout, vert_module, frag_module,
            "imiv.ocio.preview.pipeline",
            "vkCreateGraphicsPipelines failed for OCIO preview",
            vk_state.ocio.pipeline, error_message);
        vkDestroyShaderModule(vk_state.device, frag_module, vk_state.allocator);
        vkDestroyShaderModule(vk_state.device, vert_module, vk_state.allocator);
        if (!pipeline_ok) {
            return false;
        }
        return true;
    }

}  // namespace

void
destroy_ocio_preview_resources(VulkanState& vk_state)
{
    destroy_ocio_shader_runtime(vk_state.ocio.runtime);
    if (vk_state.ocio.uniform_mapped != nullptr
        && vk_state.ocio.uniform_memory != VK_NULL_HANDLE) {
        vkUnmapMemory(vk_state.device, vk_state.ocio.uniform_memory);
        vk_state.ocio.uniform_mapped = nullptr;
    }
    for (OcioVulkanTexture& texture : vk_state.ocio.textures)
        destroy_ocio_texture(vk_state, texture);
    vk_state.ocio.textures.clear();
    if (vk_state.ocio.uniform_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk_state.device, vk_state.ocio.uniform_buffer,
                        vk_state.allocator);
        vk_state.ocio.uniform_buffer = VK_NULL_HANDLE;
    }
    if (vk_state.ocio.uniform_memory != VK_NULL_HANDLE) {
        vkFreeMemory(vk_state.device, vk_state.ocio.uniform_memory,
                     vk_state.allocator);
        vk_state.ocio.uniform_memory = VK_NULL_HANDLE;
    }
    if (vk_state.ocio.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vk_state.device, vk_state.ocio.pipeline,
                          vk_state.allocator);
        vk_state.ocio.pipeline = VK_NULL_HANDLE;
    }
    if (vk_state.ocio.pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vk_state.device, vk_state.ocio.pipeline_layout,
                                vk_state.allocator);
        vk_state.ocio.pipeline_layout = VK_NULL_HANDLE;
    }
    if (vk_state.ocio.descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk_state.device,
                                     vk_state.ocio.descriptor_set_layout,
                                     vk_state.allocator);
        vk_state.ocio.descriptor_set_layout = VK_NULL_HANDLE;
    }
    if (vk_state.ocio.descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(vk_state.device, vk_state.ocio.descriptor_pool,
                                vk_state.allocator);
        vk_state.ocio.descriptor_pool = VK_NULL_HANDLE;
    }
    vk_state.ocio.descriptor_set      = VK_NULL_HANDLE;
    vk_state.ocio.uniform_buffer_size = 0;
    vk_state.ocio.shader_cache_id.clear();
    vk_state.ocio.ready = false;
}

bool
ensure_ocio_preview_resources(VulkanState& vk_state, VulkanTexture& texture,
                              const LoadedImage* image,
                              const PlaceholderUiState& ui_state,
                              const PreviewControls& controls,
                              std::string& error_message)
{
    error_message.clear();
    if (!controls.use_ocio)
        return true;

    OcioShaderRuntime* old_runtime = vk_state.ocio.runtime;
    if (!ensure_ocio_shader_runtime(ui_state, image, vk_state.ocio.runtime,
                                    error_message)) {
        return false;
    }

    const std::string shader_cache_id
        = vk_state.ocio.runtime != nullptr
              ? vk_state.ocio.runtime->blueprint.shader_cache_id
              : std::string();
    const bool needs_rebuild = !vk_state.ocio.ready
                               || vk_state.ocio.shader_cache_id
                                      != shader_cache_id
                               || old_runtime != vk_state.ocio.runtime;
    if (!needs_rebuild)
        return update_ocio_uniform_buffer_resource(vk_state, controls,
                                                   error_message);

    if (!quiesce_texture_preview_submission(vk_state, texture, error_message)) {
        return false;
    }
    destroy_ocio_preview_resources(vk_state);
    if (!ensure_ocio_shader_runtime(ui_state, image, vk_state.ocio.runtime,
                                    error_message)) {
        return false;
    }
    if (vk_state.ocio.runtime == nullptr)
        return true;

    if (!create_ocio_uniform_buffer_resource(
            vk_state, vk_state.ocio.runtime->blueprint.uniform_buffer_size,
            error_message)) {
        destroy_ocio_preview_resources(vk_state);
        return false;
    }

    vk_state.ocio.textures.reserve(
        vk_state.ocio.runtime->blueprint.textures.size());
    for (const OcioTextureBlueprint& texture_bp :
         vk_state.ocio.runtime->blueprint.textures) {
        vk_state.ocio.textures.emplace_back();
        if (!upload_ocio_texture(vk_state, texture_bp,
                                 vk_state.ocio.textures.back(),
                                 error_message)) {
            destroy_ocio_preview_resources(vk_state);
            return false;
        }
    }

    if (!create_ocio_descriptor_resources(vk_state, error_message)
        || !create_ocio_pipeline(vk_state, error_message)
        || !update_ocio_uniform_buffer_resource(vk_state, controls,
                                                error_message)) {
        destroy_ocio_preview_resources(vk_state);
        return false;
    }

    vk_state.ocio.shader_cache_id = shader_cache_id;
    vk_state.ocio.ready           = true;
    return true;
}

#endif

}  // namespace Imiv
