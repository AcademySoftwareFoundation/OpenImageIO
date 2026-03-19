// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_ocio.h"
#include "imiv_types.h"

#include <OpenImageIO/half.h>
#include <OpenImageIO/strutil.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace Imiv {

#if defined(IMIV_WITH_VULKAN)

namespace {

    bool read_binary_file(const std::string& path,
                          std::vector<uint32_t>& out_words,
                          std::string& error_message)
    {
        error_message.clear();
        out_words.clear();

        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in) {
            error_message
                = OIIO::Strutil::fmt::format("failed to open shader file '{}'",
                                             path);
            return false;
        }

        const std::streamsize size = in.tellg();
        if (size <= 0 || (size % 4) != 0) {
            error_message
                = OIIO::Strutil::fmt::format("invalid SPIR-V size for '{}'",
                                             path);
            return false;
        }
        out_words.resize(static_cast<size_t>(size) / sizeof(uint32_t));
        in.seekg(0, std::ios::beg);
        if (!in.read(reinterpret_cast<char*>(out_words.data()), size)) {
            error_message
                = OIIO::Strutil::fmt::format("failed to read shader file '{}'",
                                             path);
            out_words.clear();
            return false;
        }
        return true;
    }

    bool create_shader_module_from_words(
        VkDevice device, VkAllocationCallbacks* allocator,
        const uint32_t* words, size_t word_count, VkShaderModule& shader_module,
        std::string& error_message, const char* debug_name)
    {
        shader_module = VK_NULL_HANDLE;
        if (words == nullptr || word_count == 0) {
            error_message
                = OIIO::Strutil::fmt::format("missing SPIR-V words for {}",
                                             debug_name ? debug_name
                                                        : "shader");
            return false;
        }

        VkShaderModuleCreateInfo ci = {};
        ci.sType           = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize        = word_count * sizeof(uint32_t);
        ci.pCode           = words;
        const VkResult err = vkCreateShaderModule(device, &ci, allocator,
                                                  &shader_module);
        if (err != VK_SUCCESS) {
            error_message = OIIO::Strutil::fmt::format(
                "vkCreateShaderModule failed for {}",
                debug_name ? debug_name : "shader");
            return false;
        }
        return true;
    }

    bool create_shader_module_from_file(VkDevice device,
                                        VkAllocationCallbacks* allocator,
                                        const std::string& path,
                                        VkShaderModule& shader_module,
                                        std::string& error_message)
    {
        std::vector<uint32_t> words;
        if (!read_binary_file(path, words, error_message))
            return false;
        return create_shader_module_from_words(device, allocator, words.data(),
                                               words.size(), shader_module,
                                               error_message, path.c_str());
    }

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
        sampler                = VK_NULL_HANDLE;
        VkSamplerCreateInfo ci = {};
        ci.sType               = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        ci.magFilter     = blueprint.interpolation == OcioInterpolation::Nearest
                               ? VK_FILTER_NEAREST
                               : VK_FILTER_LINEAR;
        ci.minFilter     = ci.magFilter;
        ci.mipmapMode    = ci.magFilter == VK_FILTER_NEAREST
                               ? VK_SAMPLER_MIPMAP_MODE_NEAREST
                               : VK_SAMPLER_MIPMAP_MODE_LINEAR;
        ci.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        ci.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        ci.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        ci.minLod        = 0.0f;
        ci.maxLod        = 0.0f;
        ci.maxAnisotropy = 1.0f;
        const VkResult err = vkCreateSampler(vk_state.device, &ci,
                                             vk_state.allocator, &sampler);
        if (err != VK_SUCCESS) {
            error_message = "vkCreateSampler failed for OCIO texture";
            return false;
        }
        return true;
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

        VkMemoryRequirements image_reqs = {};
        vkGetImageMemoryRequirements(vk_state.device, texture.image,
                                     &image_reqs);
        uint32_t image_memory_type = 0;
        if (!find_memory_type_with_fallback(vk_state.physical_device,
                                            image_reqs.memoryTypeBits,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            image_memory_type)) {
            error_message = "failed to find memory type for OCIO texture";
            return false;
        }
        VkMemoryAllocateInfo image_alloc = {};
        image_alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        image_alloc.allocationSize  = image_reqs.size;
        image_alloc.memoryTypeIndex = image_memory_type;
        const VkResult mem_err = vkAllocateMemory(vk_state.device, &image_alloc,
                                                  vk_state.allocator,
                                                  &texture.memory);
        if (mem_err != VK_SUCCESS) {
            error_message = "vkAllocateMemory failed for OCIO texture";
            return false;
        }
        vkBindImageMemory(vk_state.device, texture.image, texture.memory, 0);

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

        VkMemoryRequirements buffer_reqs = {};
        vkGetBufferMemoryRequirements(vk_state.device, staging_buffer,
                                      &buffer_reqs);
        uint32_t staging_type = 0;
        if (!find_memory_type(vk_state.physical_device,
                              buffer_reqs.memoryTypeBits,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              staging_type)) {
            vkDestroyBuffer(vk_state.device, staging_buffer,
                            vk_state.allocator);
            error_message
                = "failed to find host-visible memory for OCIO texture";
            return false;
        }
        VkMemoryAllocateInfo staging_alloc = {};
        staging_alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        staging_alloc.allocationSize  = buffer_reqs.size;
        staging_alloc.memoryTypeIndex = staging_type;
        const VkResult staging_err
            = vkAllocateMemory(vk_state.device, &staging_alloc,
                               vk_state.allocator, &staging_memory);
        if (staging_err != VK_SUCCESS) {
            vkDestroyBuffer(vk_state.device, staging_buffer,
                            vk_state.allocator);
            error_message = "vkAllocateMemory failed for OCIO staging texture";
            return false;
        }
        vkBindBufferMemory(vk_state.device, staging_buffer, staging_memory, 0);

        void* mapped = nullptr;
        vkMapMemory(vk_state.device, staging_memory, 0, upload_bytes.size(), 0,
                    &mapped);
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
        const VkResult view_err = vkCreateImageView(vk_state.device, &view_ci,
                                                    vk_state.allocator,
                                                    &texture.view);
        if (view_err != VK_SUCCESS) {
            error_message = "vkCreateImageView failed for OCIO texture";
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

        VkMemoryRequirements reqs = {};
        vkGetBufferMemoryRequirements(vk_state.device,
                                      vk_state.ocio.uniform_buffer, &reqs);
        uint32_t memory_type = 0;
        if (!find_memory_type(vk_state.physical_device, reqs.memoryTypeBits,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              memory_type)) {
            error_message = "failed to find memory type for OCIO uniform buffer";
            return false;
        }

        VkMemoryAllocateInfo alloc = {};
        alloc.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize       = reqs.size;
        alloc.memoryTypeIndex      = memory_type;
        const VkResult mem_err
            = vkAllocateMemory(vk_state.device, &alloc, vk_state.allocator,
                               &vk_state.ocio.uniform_memory);
        if (mem_err != VK_SUCCESS) {
            error_message = "vkAllocateMemory failed for OCIO uniform buffer";
            return false;
        }
        vkBindBufferMemory(vk_state.device, vk_state.ocio.uniform_buffer,
                           vk_state.ocio.uniform_memory, 0);
        const VkResult map_err
            = vkMapMemory(vk_state.device, vk_state.ocio.uniform_memory, 0,
                          size, 0, &vk_state.ocio.uniform_mapped);
        if (map_err != VK_SUCCESS) {
            error_message = "vkMapMemory failed for OCIO uniform buffer";
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

        VkDescriptorSetLayoutBinding ubo_binding = {};
        ubo_binding.binding                      = 0;
        ubo_binding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_binding.descriptorCount = 1;
        ubo_binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(ubo_binding);

        for (const OcioTextureBlueprint& texture : blueprint.textures) {
            VkDescriptorSetLayoutBinding binding = {};
            binding.binding                      = texture.shader_binding;
            binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
            bindings.push_back(binding);
        }

        VkDescriptorPoolSize pool_sizes[2] = {};
        pool_sizes[0].type                 = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount      = 1;
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount = static_cast<uint32_t>(
            blueprint.textures.size());

        VkDescriptorPoolCreateInfo pool_ci = {};
        pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_ci.maxSets       = 1;
        pool_ci.poolSizeCount = blueprint.textures.empty() ? 1u : 2u;
        pool_ci.pPoolSizes    = pool_sizes;
        const VkResult pool_err
            = vkCreateDescriptorPool(vk_state.device, &pool_ci,
                                     vk_state.allocator,
                                     &vk_state.ocio.descriptor_pool);
        if (pool_err != VK_SUCCESS) {
            error_message = "vkCreateDescriptorPool failed for OCIO";
            return false;
        }

        VkDescriptorSetLayoutCreateInfo layout_ci = {};
        layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_ci.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_ci.pBindings    = bindings.data();
        const VkResult layout_err
            = vkCreateDescriptorSetLayout(vk_state.device, &layout_ci,
                                          vk_state.allocator,
                                          &vk_state.ocio.descriptor_set_layout);
        if (layout_err != VK_SUCCESS) {
            error_message = "vkCreateDescriptorSetLayout failed for OCIO";
            return false;
        }

        VkDescriptorSetAllocateInfo alloc = {};
        alloc.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.descriptorPool = vk_state.ocio.descriptor_pool;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts        = &vk_state.ocio.descriptor_set_layout;
        const VkResult set_err
            = vkAllocateDescriptorSets(vk_state.device, &alloc,
                                       &vk_state.ocio.descriptor_set);
        if (set_err != VK_SUCCESS) {
            error_message = "vkAllocateDescriptorSets failed for OCIO";
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

        VkWriteDescriptorSet ubo_write = {};
        ubo_write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ubo_write.dstSet               = vk_state.ocio.descriptor_set;
        ubo_write.dstBinding           = 0;
        ubo_write.descriptorCount      = 1;
        ubo_write.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_write.pBufferInfo          = &buffer_info;
        writes.push_back(ubo_write);

        for (const OcioVulkanTexture& texture : vk_state.ocio.textures) {
            VkDescriptorImageInfo info = {};
            info.sampler               = texture.sampler;
            info.imageView             = texture.view;
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_infos.push_back(info);
        }
        for (size_t i = 0; i < vk_state.ocio.textures.size(); ++i) {
            VkWriteDescriptorSet write = {};
            write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet               = vk_state.ocio.descriptor_set;
            write.dstBinding           = vk_state.ocio.textures[i].binding;
            write.descriptorCount      = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo     = &image_infos[i];
            writes.push_back(write);
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
        if (!create_shader_module_from_file(vk_state.device, vk_state.allocator,
                                            shader_vert, vert_module,
                                            error_message)) {
            return false;
        }
        if (!create_shader_module_from_words(
                vk_state.device, vk_state.allocator, frag_words.data(),
                frag_words.size(), frag_module, error_message,
                "imiv.ocio.preview.frag")) {
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

        VkPipelineLayoutCreateInfo layout_ci = {};
        layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_ci.setLayoutCount         = 2;
        layout_ci.pSetLayouts            = set_layouts;
        layout_ci.pushConstantRangeCount = 1;
        layout_ci.pPushConstantRanges    = &push;
        const VkResult layout_err
            = vkCreatePipelineLayout(vk_state.device, &layout_ci,
                                     vk_state.allocator,
                                     &vk_state.ocio.pipeline_layout);
        if (layout_err != VK_SUCCESS) {
            error_message = "vkCreatePipelineLayout failed for OCIO preview";
            vkDestroyShaderModule(vk_state.device, frag_module,
                                  vk_state.allocator);
            vkDestroyShaderModule(vk_state.device, vert_module,
                                  vk_state.allocator);
            return false;
        }

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert_module;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag_module;
        stages[1].pName  = "main";

        VkPipelineVertexInputStateCreateInfo vertex_input = {};
        vertex_input.sType
            = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
        input_assembly.sType
            = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport_state = {};
        viewport_state.sType
            = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount                  = 1;
        viewport_state.scissorCount                   = 1;
        VkPipelineRasterizationStateCreateInfo raster = {};
        raster.sType
            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode    = VK_CULL_MODE_NONE;
        raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth   = 1.0f;
        VkPipelineMultisampleStateCreateInfo multisample = {};
        multisample.sType
            = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples               = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState attachment = {};
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                                    | VK_COLOR_COMPONENT_G_BIT
                                    | VK_COLOR_COMPONENT_B_BIT
                                    | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend = {};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount                    = 1;
        blend.pAttachments                       = &attachment;
        VkDynamicState dynamic_states[]          = { VK_DYNAMIC_STATE_VIEWPORT,
                                                     VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic = {};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates    = dynamic_states;

        VkGraphicsPipelineCreateInfo pipeline_ci = {};
        pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_ci.stageCount          = 2;
        pipeline_ci.pStages             = stages;
        pipeline_ci.pVertexInputState   = &vertex_input;
        pipeline_ci.pInputAssemblyState = &input_assembly;
        pipeline_ci.pViewportState      = &viewport_state;
        pipeline_ci.pRasterizationState = &raster;
        pipeline_ci.pMultisampleState   = &multisample;
        pipeline_ci.pColorBlendState    = &blend;
        pipeline_ci.pDynamicState       = &dynamic;
        pipeline_ci.layout              = vk_state.ocio.pipeline_layout;
        pipeline_ci.renderPass          = vk_state.preview_render_pass;
        pipeline_ci.subpass             = 0;
        const VkResult pipeline_err     = vkCreateGraphicsPipelines(
            vk_state.device, vk_state.pipeline_cache, 1, &pipeline_ci,
            vk_state.allocator, &vk_state.ocio.pipeline);
        vkDestroyShaderModule(vk_state.device, frag_module, vk_state.allocator);
        vkDestroyShaderModule(vk_state.device, vert_module, vk_state.allocator);
        if (pipeline_err != VK_SUCCESS) {
            error_message = "vkCreateGraphicsPipelines failed for OCIO preview";
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
