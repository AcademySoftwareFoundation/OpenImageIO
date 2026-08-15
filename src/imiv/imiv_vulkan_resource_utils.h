// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_vulkan_types.h"

#include <string>

namespace Imiv {

#if IMIV_WITH_VULKAN

bool
allocate_and_bind_image_memory(
    VulkanState& vk_state, VkImage image,
    VkMemoryPropertyFlags preferred_properties, bool allow_property_fallback,
    VkDeviceMemory& memory, const char* no_memory_type_error,
    const char* allocate_error, const char* bind_error,
    const char* debug_memory_name, std::string& error_message);
bool
allocate_and_bind_buffer_memory(
    VulkanState& vk_state, VkBuffer buffer,
    VkMemoryPropertyFlags preferred_properties, bool allow_property_fallback,
    VkDeviceMemory& memory, const char* no_memory_type_error,
    const char* allocate_error, const char* bind_error,
    const char* debug_memory_name, std::string& error_message);
bool
create_buffer_resource(VulkanState& vk_state, VkDeviceSize size,
                       VkBufferUsageFlags usage, VkBuffer& buffer,
                       const char* create_error, const char* debug_name,
                       std::string& error_message);
bool
create_buffer_with_memory_resource(
    VulkanState& vk_state, VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags preferred_properties, bool allow_property_fallback,
    VkBuffer& buffer, VkDeviceMemory& memory, const char* create_error,
    const char* no_memory_type_error, const char* allocate_error,
    const char* bind_error, const char* debug_buffer_name,
    const char* debug_memory_name, std::string& error_message);
bool
create_image_view_resource(VulkanState& vk_state,
                           const VkImageViewCreateInfo& create_info,
                           VkImageView& image_view, const char* create_error,
                           const char* debug_name, std::string& error_message);
VkSamplerCreateInfo
make_clamped_sampler_create_info(VkFilter min_filter, VkFilter mag_filter,
                                 VkSamplerMipmapMode mipmap_mode, float min_lod,
                                 float max_lod);
bool
create_sampler_resource(VulkanState& vk_state,
                        const VkSamplerCreateInfo& create_info,
                        VkSampler& sampler, const char* create_error,
                        const char* debug_name, std::string& error_message);
VkDescriptorSetLayoutBinding
make_descriptor_set_layout_binding(uint32_t binding,
                                   VkDescriptorType descriptor_type,
                                   VkShaderStageFlags stage_flags);
bool
create_descriptor_pool_resource(
    VulkanState& vk_state, VkDescriptorPoolCreateFlags flags, uint32_t max_sets,
    const VkDescriptorPoolSize* pool_sizes, uint32_t pool_size_count,
    VkDescriptorPool& descriptor_pool, const char* create_error,
    const char* debug_name, std::string& error_message);
bool
create_descriptor_set_layout_resource(
    VulkanState& vk_state, const VkDescriptorSetLayoutBinding* bindings,
    uint32_t binding_count, VkDescriptorSetLayout& descriptor_set_layout,
    const char* create_error, const char* debug_name,
    std::string& error_message);
bool
allocate_descriptor_set_resource(VulkanState& vk_state,
                                 VkDescriptorPool descriptor_pool,
                                 VkDescriptorSetLayout descriptor_set_layout,
                                 VkDescriptorSet& descriptor_set,
                                 const char* allocate_error,
                                 std::string& error_message);
bool
create_pipeline_layout_resource(
    VulkanState& vk_state, const VkDescriptorSetLayout* set_layouts,
    uint32_t set_layout_count, const VkPushConstantRange* push_constant_ranges,
    uint32_t push_constant_range_count, VkPipelineLayout& pipeline_layout,
    const char* create_error, const char* debug_name,
    std::string& error_message);
VkWriteDescriptorSet
make_buffer_descriptor_write(VkDescriptorSet descriptor_set, uint32_t binding,
                             VkDescriptorType descriptor_type,
                             const VkDescriptorBufferInfo* buffer_info);
VkWriteDescriptorSet
make_image_descriptor_write(VkDescriptorSet descriptor_set, uint32_t binding,
                            VkDescriptorType descriptor_type,
                            const VkDescriptorImageInfo* image_info);
VkImageMemoryBarrier
make_color_image_memory_barrier(VkImage image, VkImageLayout old_layout,
                                VkImageLayout new_layout,
                                VkAccessFlags src_access_mask,
                                VkAccessFlags dst_access_mask);
bool
nonblocking_fence_status(VkDevice device, VkFence fence, const char* context,
                         bool& out_signaled, std::string& error_message);
bool
ensure_async_submit_resources(
    VulkanState& vk_state, VkCommandPool& command_pool,
    VkCommandBuffer& command_buffer, VkFence& submit_fence,
    const char* command_pool_error, const char* command_buffer_error,
    const char* fence_error, const char* command_pool_debug_name,
    const char* command_buffer_debug_name, const char* fence_debug_name,
    std::string& error_message);
void
destroy_async_submit_resources(VulkanState& vk_state,
                               VkCommandPool& command_pool,
                               VkCommandBuffer& command_buffer,
                               VkFence& submit_fence);
bool
map_memory_resource(VulkanState& vk_state, VkDeviceMemory memory,
                    VkDeviceSize size, void*& mapped, const char* map_error,
                    std::string& error_message);

#endif

}  // namespace Imiv
