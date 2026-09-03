// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_vulkan_shader_utils.h"

#include <fstream>

#include <OpenImageIO/strutil.h>

namespace Imiv {

#if defined(IMIV_WITH_VULKAN)

bool
read_spirv_words(const std::string& path, std::vector<uint32_t>& out_words,
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
            = OIIO::Strutil::fmt::format("invalid SPIR-V size for '{}'", path);
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

bool
create_shader_module_from_words(VkDevice device,
                                VkAllocationCallbacks* allocator,
                                const uint32_t* words, size_t word_count,
                                const char* debug_name,
                                VkShaderModule& shader_module,
                                std::string& error_message)
{
    shader_module = VK_NULL_HANDLE;
    if (words == nullptr || word_count == 0) {
        error_message
            = OIIO::Strutil::fmt::format("missing SPIR-V words for {}",
                                         debug_name ? debug_name : "shader");
        return false;
    }

    VkShaderModuleCreateInfo ci = {};
    ci.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize                 = word_count * sizeof(uint32_t);
    ci.pCode                    = words;
    const VkResult err          = vkCreateShaderModule(device, &ci, allocator,
                                                       &shader_module);
    if (err != VK_SUCCESS) {
        error_message
            = OIIO::Strutil::fmt::format("vkCreateShaderModule failed for {}",
                                         debug_name ? debug_name : "shader");
        return false;
    }

    return true;
}

bool
create_shader_module_from_file(VkDevice device,
                               VkAllocationCallbacks* allocator,
                               const std::string& path,
                               VkShaderModule& shader_module,
                               std::string& error_message)
{
    std::vector<uint32_t> words;
    if (!read_spirv_words(path, words, error_message))
        return false;

    return create_shader_module_from_words(device, allocator, words.data(),
                                           words.size(), path.c_str(),
                                           shader_module, error_message);
}

bool
create_shader_module_from_embedded_or_file(
    VkDevice device, VkAllocationCallbacks* allocator, const uint32_t* words,
    size_t word_count, const std::string& path, const char* debug_name,
    VkShaderModule& shader_module, std::string& error_message)
{
    if (words != nullptr && word_count != 0) {
        return create_shader_module_from_words(device, allocator, words,
                                               word_count, debug_name,
                                               shader_module, error_message);
    }

    return create_shader_module_from_file(device, allocator, path,
                                          shader_module, error_message);
}

bool
create_fullscreen_preview_pipeline(
    VulkanState& vk_state, VkRenderPass render_pass,
    VkPipelineLayout pipeline_layout, VkShaderModule vert_module,
    VkShaderModule frag_module, const char* debug_name,
    const char* create_error, VkPipeline& pipeline, std::string& error_message)
{
    pipeline = VK_NULL_HANDLE;

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
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount                  = 1;
    viewport_state.scissorCount                   = 1;
    VkPipelineRasterizationStateCreateInfo raster = {};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode    = VK_CULL_MODE_NONE;
    raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth   = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                                            | VK_COLOR_COMPONENT_G_BIT
                                            | VK_COLOR_COMPONENT_B_BIT
                                            | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo color_blend = {};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount     = 1;
    color_blend.pAttachments        = &color_blend_attachment;
    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT,
                                        VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(
        IM_ARRAYSIZE(dynamic_states));
    dynamic_state.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo pipeline_ci = {};
    pipeline_ci.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.stageCount = 2;
    pipeline_ci.pStages    = stages;
    pipeline_ci.pVertexInputState   = &vertex_input;
    pipeline_ci.pInputAssemblyState = &input_assembly;
    pipeline_ci.pViewportState      = &viewport_state;
    pipeline_ci.pRasterizationState = &raster;
    pipeline_ci.pMultisampleState   = &multisample;
    pipeline_ci.pColorBlendState    = &color_blend;
    pipeline_ci.pDynamicState       = &dynamic_state;
    pipeline_ci.layout              = pipeline_layout;
    pipeline_ci.renderPass          = render_pass;
    pipeline_ci.subpass             = 0;

    const VkResult err
        = vkCreateGraphicsPipelines(vk_state.device, vk_state.pipeline_cache, 1,
                                    &pipeline_ci, vk_state.allocator,
                                    &pipeline);
    if (err != VK_SUCCESS) {
        error_message = create_error;
        return false;
    }

    set_vk_object_name(vk_state, VK_OBJECT_TYPE_PIPELINE, pipeline, debug_name);
    return true;
}

#endif

}  // namespace Imiv
