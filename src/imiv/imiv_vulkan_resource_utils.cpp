// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_vulkan_resource_utils.h"

namespace Imiv {

#if defined(IMIV_WITH_VULKAN)

namespace {

    bool allocate_memory_resource(
        VulkanState& vk_state, uint32_t memory_type_bits,
        VkMemoryPropertyFlags preferred_properties,
        bool allow_property_fallback, VkDeviceSize allocation_size,
        VkDeviceMemory& memory, const char* no_memory_type_error,
        const char* allocate_error, const char* debug_memory_name,
        std::string& error_message)
    {
        memory = VK_NULL_HANDLE;

        uint32_t memory_type_index = 0;
        const bool have_memory_type
            = allow_property_fallback
                  ? find_memory_type_with_fallback(vk_state.physical_device,
                                                   memory_type_bits,
                                                   preferred_properties,
                                                   memory_type_index)
                  : find_memory_type(vk_state.physical_device, memory_type_bits,
                                     preferred_properties, memory_type_index);
        if (!have_memory_type) {
            error_message = no_memory_type_error;
            return false;
        }

        VkMemoryAllocateInfo alloc = {};
        alloc.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize       = allocation_size;
        alloc.memoryTypeIndex      = memory_type_index;
        const VkResult alloc_err   = vkAllocateMemory(vk_state.device, &alloc,
                                                      vk_state.allocator,
                                                      &memory);
        if (alloc_err != VK_SUCCESS) {
            error_message = allocate_error;
            return false;
        }
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_DEVICE_MEMORY, memory,
                           debug_memory_name);
        return true;
    }

}  // namespace

bool
allocate_and_bind_image_memory(
    VulkanState& vk_state, VkImage image,
    VkMemoryPropertyFlags preferred_properties, bool allow_property_fallback,
    VkDeviceMemory& memory, const char* no_memory_type_error,
    const char* allocate_error, const char* bind_error,
    const char* debug_memory_name, std::string& error_message)
{
    VkMemoryRequirements requirements = {};
    vkGetImageMemoryRequirements(vk_state.device, image, &requirements);
    if (!allocate_memory_resource(vk_state, requirements.memoryTypeBits,
                                  preferred_properties, allow_property_fallback,
                                  requirements.size, memory,
                                  no_memory_type_error, allocate_error,
                                  debug_memory_name, error_message)) {
        return false;
    }

    const VkResult bind_err = vkBindImageMemory(vk_state.device, image, memory,
                                                0);
    if (bind_err != VK_SUCCESS) {
        error_message = bind_error;
        return false;
    }

    return true;
}

bool
allocate_and_bind_buffer_memory(
    VulkanState& vk_state, VkBuffer buffer,
    VkMemoryPropertyFlags preferred_properties, bool allow_property_fallback,
    VkDeviceMemory& memory, const char* no_memory_type_error,
    const char* allocate_error, const char* bind_error,
    const char* debug_memory_name, std::string& error_message)
{
    VkMemoryRequirements requirements = {};
    vkGetBufferMemoryRequirements(vk_state.device, buffer, &requirements);
    if (!allocate_memory_resource(vk_state, requirements.memoryTypeBits,
                                  preferred_properties, allow_property_fallback,
                                  requirements.size, memory,
                                  no_memory_type_error, allocate_error,
                                  debug_memory_name, error_message)) {
        return false;
    }

    const VkResult bind_err = vkBindBufferMemory(vk_state.device, buffer,
                                                 memory, 0);
    if (bind_err != VK_SUCCESS) {
        error_message = bind_error;
        return false;
    }

    return true;
}

bool
create_image_view_resource(VulkanState& vk_state,
                           const VkImageViewCreateInfo& create_info,
                           VkImageView& image_view, const char* create_error,
                           const char* debug_name, std::string& error_message)
{
    image_view         = VK_NULL_HANDLE;
    const VkResult err = vkCreateImageView(vk_state.device, &create_info,
                                           vk_state.allocator, &image_view);
    if (err != VK_SUCCESS) {
        error_message = create_error;
        return false;
    }

    set_vk_object_name(vk_state, VK_OBJECT_TYPE_IMAGE_VIEW, image_view,
                       debug_name);
    return true;
}

VkSamplerCreateInfo
make_clamped_sampler_create_info(VkFilter min_filter, VkFilter mag_filter,
                                 VkSamplerMipmapMode mipmap_mode,
                                 float min_lod, float max_lod)
{
    VkSamplerCreateInfo create_info = {};
    create_info.sType               = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    create_info.minFilter           = min_filter;
    create_info.magFilter           = mag_filter;
    create_info.mipmapMode          = mipmap_mode;
    create_info.addressModeU        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    create_info.addressModeV        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    create_info.addressModeW        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    create_info.minLod              = min_lod;
    create_info.maxLod              = max_lod;
    create_info.maxAnisotropy       = 1.0f;
    return create_info;
}

bool
create_sampler_resource(VulkanState& vk_state,
                        const VkSamplerCreateInfo& create_info,
                        VkSampler& sampler, const char* create_error,
                        const char* debug_name, std::string& error_message)
{
    sampler             = VK_NULL_HANDLE;
    const VkResult err  = vkCreateSampler(vk_state.device, &create_info,
                                          vk_state.allocator, &sampler);
    if (err != VK_SUCCESS) {
        error_message = create_error;
        return false;
    }

    if (debug_name != nullptr) {
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_SAMPLER, sampler,
                           debug_name);
    }
    return true;
}

VkDescriptorSetLayoutBinding
make_descriptor_set_layout_binding(uint32_t binding,
                                   VkDescriptorType descriptor_type,
                                   VkShaderStageFlags stage_flags)
{
    VkDescriptorSetLayoutBinding layout_binding = {};
    layout_binding.binding                      = binding;
    layout_binding.descriptorType               = descriptor_type;
    layout_binding.descriptorCount              = 1;
    layout_binding.stageFlags                   = stage_flags;
    return layout_binding;
}

bool
create_descriptor_pool_resource(
    VulkanState& vk_state, VkDescriptorPoolCreateFlags flags, uint32_t max_sets,
    const VkDescriptorPoolSize* pool_sizes, uint32_t pool_size_count,
    VkDescriptorPool& descriptor_pool, const char* create_error,
    const char* debug_name, std::string& error_message)
{
    descriptor_pool                 = VK_NULL_HANDLE;
    VkDescriptorPoolCreateInfo pool = {};
    pool.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.flags         = flags;
    pool.maxSets       = max_sets;
    pool.poolSizeCount = pool_size_count;
    pool.pPoolSizes    = pool_sizes;
    const VkResult err = vkCreateDescriptorPool(vk_state.device, &pool,
                                                vk_state.allocator,
                                                &descriptor_pool);
    if (err != VK_SUCCESS) {
        error_message = create_error;
        return false;
    }

    set_vk_object_name(vk_state, VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                       descriptor_pool, debug_name);
    return true;
}

bool
create_descriptor_set_layout_resource(
    VulkanState& vk_state, const VkDescriptorSetLayoutBinding* bindings,
    uint32_t binding_count, VkDescriptorSetLayout& descriptor_set_layout,
    const char* create_error, const char* debug_name,
    std::string& error_message)
{
    descriptor_set_layout                  = VK_NULL_HANDLE;
    VkDescriptorSetLayoutCreateInfo layout = {};
    layout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout.bindingCount = binding_count;
    layout.pBindings    = bindings;
    const VkResult err  = vkCreateDescriptorSetLayout(vk_state.device, &layout,
                                                      vk_state.allocator,
                                                      &descriptor_set_layout);
    if (err != VK_SUCCESS) {
        error_message = create_error;
        return false;
    }

    set_vk_object_name(vk_state, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                       descriptor_set_layout, debug_name);
    return true;
}

bool
allocate_descriptor_set_resource(VulkanState& vk_state,
                                 VkDescriptorPool descriptor_pool,
                                 VkDescriptorSetLayout descriptor_set_layout,
                                 VkDescriptorSet& descriptor_set,
                                 const char* allocate_error,
                                 std::string& error_message)
{
    descriptor_set                       = VK_NULL_HANDLE;
    VkDescriptorSetAllocateInfo allocate = {};
    allocate.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate.descriptorPool = descriptor_pool;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts        = &descriptor_set_layout;
    const VkResult err = vkAllocateDescriptorSets(vk_state.device, &allocate,
                                                  &descriptor_set);
    if (err != VK_SUCCESS) {
        error_message = allocate_error;
        return false;
    }

    return true;
}

bool
create_pipeline_layout_resource(
    VulkanState& vk_state, const VkDescriptorSetLayout* set_layouts,
    uint32_t set_layout_count, const VkPushConstantRange* push_constant_ranges,
    uint32_t push_constant_range_count, VkPipelineLayout& pipeline_layout,
    const char* create_error, const char* debug_name,
    std::string& error_message)
{
    pipeline_layout                    = VK_NULL_HANDLE;
    VkPipelineLayoutCreateInfo layout  = {};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.setLayoutCount              = set_layout_count;
    layout.pSetLayouts                 = set_layouts;
    layout.pushConstantRangeCount      = push_constant_range_count;
    layout.pPushConstantRanges         = push_constant_ranges;
    const VkResult err = vkCreatePipelineLayout(vk_state.device, &layout,
                                                vk_state.allocator,
                                                &pipeline_layout);
    if (err != VK_SUCCESS) {
        error_message = create_error;
        return false;
    }

    if (debug_name != nullptr) {
        set_vk_object_name(vk_state, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                           pipeline_layout, debug_name);
    }
    return true;
}

VkWriteDescriptorSet
make_buffer_descriptor_write(VkDescriptorSet descriptor_set, uint32_t binding,
                             VkDescriptorType descriptor_type,
                             const VkDescriptorBufferInfo* buffer_info)
{
    VkWriteDescriptorSet write = {};
    write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet               = descriptor_set;
    write.dstBinding           = binding;
    write.descriptorCount      = 1;
    write.descriptorType       = descriptor_type;
    write.pBufferInfo          = buffer_info;
    return write;
}

VkWriteDescriptorSet
make_image_descriptor_write(VkDescriptorSet descriptor_set, uint32_t binding,
                            VkDescriptorType descriptor_type,
                            const VkDescriptorImageInfo* image_info)
{
    VkWriteDescriptorSet write = {};
    write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet               = descriptor_set;
    write.dstBinding           = binding;
    write.descriptorCount      = 1;
    write.descriptorType       = descriptor_type;
    write.pImageInfo           = image_info;
    return write;
}

bool
nonblocking_fence_status(VkDevice device, VkFence fence, const char* context,
                         bool& out_signaled, std::string& error_message)
{
    out_signaled = false;
    if (fence == VK_NULL_HANDLE) {
        error_message = std::string(context) + " fence is unavailable";
        return false;
    }

    const VkResult err = vkGetFenceStatus(device, fence);
    if (err == VK_SUCCESS) {
        out_signaled = true;
        return true;
    }
    if (err == VK_NOT_READY)
        return true;

    error_message = std::string("vkGetFenceStatus failed for ") + context;
    check_vk_result(err);
    return false;
}

bool
ensure_async_submit_resources(
    VulkanState& vk_state, VkCommandPool& command_pool,
    VkCommandBuffer& command_buffer, VkFence& submit_fence,
    const char* command_pool_error, const char* command_buffer_error,
    const char* fence_error, const char* command_pool_debug_name,
    const char* command_buffer_debug_name, const char* fence_debug_name,
    std::string& error_message)
{
    if (command_pool != VK_NULL_HANDLE && command_buffer != VK_NULL_HANDLE
        && submit_fence != VK_NULL_HANDLE) {
        return true;
    }

    destroy_async_submit_resources(vk_state, command_pool, command_buffer,
                                   submit_fence);

    VkCommandPool new_command_pool     = VK_NULL_HANDLE;
    VkCommandBuffer new_command_buffer = VK_NULL_HANDLE;
    VkFence new_submit_fence           = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo pool_ci = {};
    pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
                    | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_ci.queueFamilyIndex = vk_state.queue_family;
    VkResult err             = vkCreateCommandPool(vk_state.device, &pool_ci,
                                                   vk_state.allocator, &new_command_pool);
    if (err != VK_SUCCESS) {
        error_message = command_pool_error;
        return false;
    }
    set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_POOL, new_command_pool,
                       command_pool_debug_name);

    VkCommandBufferAllocateInfo command_alloc = {};
    command_alloc.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_alloc.commandPool = new_command_pool;
    command_alloc.level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_alloc.commandBufferCount = 1;
    err = vkAllocateCommandBuffers(vk_state.device, &command_alloc,
                                   &new_command_buffer);
    if (err != VK_SUCCESS) {
        vkDestroyCommandPool(vk_state.device, new_command_pool,
                             vk_state.allocator);
        error_message = command_buffer_error;
        return false;
    }
    set_vk_object_name(vk_state, VK_OBJECT_TYPE_COMMAND_BUFFER,
                       new_command_buffer, command_buffer_debug_name);

    VkFenceCreateInfo fence_ci = {};
    fence_ci.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
    err = vkCreateFence(vk_state.device, &fence_ci, vk_state.allocator,
                        &new_submit_fence);
    if (err != VK_SUCCESS) {
        vkDestroyCommandPool(vk_state.device, new_command_pool,
                             vk_state.allocator);
        error_message = fence_error;
        return false;
    }
    set_vk_object_name(vk_state, VK_OBJECT_TYPE_FENCE, new_submit_fence,
                       fence_debug_name);

    command_pool   = new_command_pool;
    command_buffer = new_command_buffer;
    submit_fence   = new_submit_fence;
    return true;
}

void
destroy_async_submit_resources(VulkanState& vk_state,
                               VkCommandPool& command_pool,
                               VkCommandBuffer& command_buffer,
                               VkFence& submit_fence)
{
    if (submit_fence != VK_NULL_HANDLE) {
        vkDestroyFence(vk_state.device, submit_fence, vk_state.allocator);
        submit_fence = VK_NULL_HANDLE;
    }
    command_buffer = VK_NULL_HANDLE;
    if (command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(vk_state.device, command_pool, vk_state.allocator);
        command_pool = VK_NULL_HANDLE;
    }
}

bool
map_memory_resource(VulkanState& vk_state, VkDeviceMemory memory,
                    VkDeviceSize size, void*& mapped, const char* map_error,
                    std::string& error_message)
{
    mapped             = nullptr;
    const VkResult err = vkMapMemory(vk_state.device, memory, 0, size, 0,
                                     &mapped);
    if (err != VK_SUCCESS || mapped == nullptr) {
        error_message = map_error;
        mapped        = nullptr;
        return false;
    }

    return true;
}

#endif

}  // namespace Imiv
