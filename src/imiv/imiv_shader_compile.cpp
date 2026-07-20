// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "imiv_shader_compile.h"

#if defined(IMIV_HAS_GLSLANG_RUNTIME) && IMIV_HAS_GLSLANG_RUNTIME
#    include <glslang/Include/glslang_c_interface.h>
#    include <glslang/Public/resource_limits_c.h>
#endif

namespace Imiv {

namespace {

#if defined(IMIV_HAS_GLSLANG_RUNTIME) && IMIV_HAS_GLSLANG_RUNTIME

    glslang_stage_t map_shader_stage(RuntimeShaderStage stage)
    {
        switch (stage) {
        case RuntimeShaderStage::Vertex: return GLSLANG_STAGE_VERTEX;
        case RuntimeShaderStage::Fragment: return GLSLANG_STAGE_FRAGMENT;
        case RuntimeShaderStage::Compute: return GLSLANG_STAGE_COMPUTE;
        default: return GLSLANG_STAGE_FRAGMENT;
        }
    }

    bool append_glslang_log(const char* text, std::string& dst)
    {
        if (text == nullptr || text[0] == '\0')
            return false;
        if (!dst.empty())
            dst += "\n";
        dst += text;
        return true;
    }

#endif

}  // namespace

bool
compile_glsl_to_spirv(RuntimeShaderStage stage, const std::string& source_code,
                      const char* debug_name,
                      std::vector<uint32_t>& spirv_words,
                      std::string& error_message)
{
    spirv_words.clear();
    error_message.clear();

#if !(defined(IMIV_HAS_GLSLANG_RUNTIME) && IMIV_HAS_GLSLANG_RUNTIME)
    (void)stage;
    (void)source_code;
    (void)debug_name;
    error_message = "runtime GLSL compiler is unavailable in this build";
    return false;
#else
    if (source_code.empty()) {
        error_message = "runtime GLSL compiler received empty source";
        return false;
    }

    if (!glslang_initialize_process()) {
        error_message = "glslang_initialize_process failed";
        return false;
    }

    const glslang_resource_t* resources     = glslang_default_resource();
    glslang_input_t input                   = {};
    input.language                          = GLSLANG_SOURCE_GLSL;
    input.stage                             = map_shader_stage(stage);
    input.client                            = GLSLANG_CLIENT_VULKAN;
    input.client_version                    = GLSLANG_TARGET_VULKAN_1_2;
    input.target_language                   = GLSLANG_TARGET_SPV;
    input.target_language_version           = GLSLANG_TARGET_SPV_1_5;
    input.code                              = source_code.c_str();
    input.default_version                   = 460;
    input.default_profile                   = GLSLANG_CORE_PROFILE;
    input.force_default_version_and_profile = 0;
    input.forward_compatible                = 0;
    input.messages                          = static_cast<glslang_messages_t>(
        GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT);
    input.resource = resources;

    glslang_shader_t* shader = glslang_shader_create(&input);
    if (shader == nullptr) {
        glslang_finalize_process();
        error_message = "glslang_shader_create failed";
        return false;
    }

    bool ok = true;
    if (!glslang_shader_preprocess(shader, &input))
        ok = false;
    if (ok && !glslang_shader_parse(shader, &input))
        ok = false;
    if (!ok) {
        if (debug_name != nullptr && debug_name[0] != '\0') {
            error_message += debug_name;
            error_message += ": ";
        }
        error_message += "GLSL preprocess/parse failed";
        append_glslang_log(glslang_shader_get_info_log(shader), error_message);
        append_glslang_log(glslang_shader_get_info_debug_log(shader),
                           error_message);
        glslang_shader_delete(shader);
        glslang_finalize_process();
        return false;
    }

    glslang_program_t* program = glslang_program_create();
    if (program == nullptr) {
        glslang_shader_delete(shader);
        glslang_finalize_process();
        error_message = "glslang_program_create failed";
        return false;
    }
    glslang_program_add_shader(program, shader);

    ok = glslang_program_link(program, static_cast<glslang_messages_t>(
                                           GLSLANG_MSG_SPV_RULES_BIT
                                           | GLSLANG_MSG_VULKAN_RULES_BIT))
         != 0;
    if (ok)
        ok = glslang_program_map_io(program) != 0;
    if (!ok) {
        if (debug_name != nullptr && debug_name[0] != '\0') {
            error_message += debug_name;
            error_message += ": ";
        }
        error_message += "GLSL program link/map failed";
        append_glslang_log(glslang_program_get_info_log(program),
                           error_message);
        append_glslang_log(glslang_program_get_info_debug_log(program),
                           error_message);
        glslang_program_delete(program);
        glslang_shader_delete(shader);
        glslang_finalize_process();
        return false;
    }

    glslang_spv_options_t spv_options                = {};
    spv_options.generate_debug_info                  = false;
    spv_options.strip_debug_info                     = true;
    spv_options.disable_optimizer                    = false;
    spv_options.optimize_size                        = false;
    spv_options.disassemble                          = false;
    spv_options.validate                             = true;
    spv_options.emit_nonsemantic_shader_debug_info   = false;
    spv_options.emit_nonsemantic_shader_debug_source = false;
    spv_options.compile_only                         = false;
    spv_options.optimize_allow_expanded_id_bound     = false;
    glslang_program_SPIRV_generate_with_options(program, input.stage,
                                                &spv_options);

    const size_t word_count = glslang_program_SPIRV_get_size(program);
    if (word_count == 0) {
        if (debug_name != nullptr && debug_name[0] != '\0') {
            error_message += debug_name;
            error_message += ": ";
        }
        error_message += "SPIR-V generation produced no output";
        append_glslang_log(glslang_program_SPIRV_get_messages(program),
                           error_message);
        glslang_program_delete(program);
        glslang_shader_delete(shader);
        glslang_finalize_process();
        return false;
    }

    spirv_words.resize(word_count);
    glslang_program_SPIRV_get(program, spirv_words.data());
    append_glslang_log(glslang_program_SPIRV_get_messages(program),
                       error_message);
    glslang_program_delete(program);
    glslang_shader_delete(shader);
    glslang_finalize_process();
    return true;
#endif
}

}  // namespace Imiv
