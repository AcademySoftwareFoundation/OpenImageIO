// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include "imiv_vulkan_types.h"

#include <string>
#include <vector>

namespace Imiv {

#if IMIV_WITH_VULKAN

bool
read_spirv_words(const std::string& path, std::vector<uint32_t>& out_words,
                 std::string& error_message);
bool
create_shader_module_from_words(VkDevice device,
                                VkAllocationCallbacks* allocator,
                                const uint32_t* words, size_t word_count,
                                const char* debug_name,
                                VkShaderModule& shader_module,
                                std::string& error_message);
bool
create_shader_module_from_file(VkDevice device,
                               VkAllocationCallbacks* allocator,
                               const std::string& path,
                               VkShaderModule& shader_module,
                               std::string& error_message);
bool
create_shader_module_from_embedded_or_file(
    VkDevice device, VkAllocationCallbacks* allocator, const uint32_t* words,
    size_t word_count, const std::string& path, const char* debug_name,
    VkShaderModule& shader_module, std::string& error_message);
bool
create_fullscreen_preview_pipeline(
    VulkanState& vk_state, VkRenderPass render_pass,
    VkPipelineLayout pipeline_layout, VkShaderModule vert_module,
    VkShaderModule frag_module, const char* debug_name,
    const char* create_error, VkPipeline& pipeline, std::string& error_message);

#endif

}  // namespace Imiv
