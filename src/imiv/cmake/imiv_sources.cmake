# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set (_imiv_shared_sources
     imiv_actions.cpp
     imiv_action_dispatch.cpp
     imiv_app.cpp
     imiv_aux_windows.cpp
     imiv_developer_tools.cpp
     imiv_file_actions.cpp
     imiv_file_dialog.cpp
     imiv_frame.cpp
     imiv_image_library.cpp
     imiv_image_view.cpp
     imiv_loaded_image.cpp
     imiv_menu.cpp
     imiv_navigation.cpp
     imiv_ocio.cpp
     imiv_parse.cpp
     imiv_persistence.cpp
     imiv_overlays.cpp
     imiv_probe_overlay.cpp
     imiv_preview_shader_text.cpp
     imiv_renderer.cpp
     imiv_shader_compile.cpp
     imiv_style.cpp
     imiv_tiling.cpp
     imiv_upload_types.cpp
     imiv_ui.cpp
     imiv_viewer.cpp
     imiv_workspace_ui.cpp
     imiv_main.cpp)

set (_imiv_test_engine_integration_sources
     imiv_test_engine.cpp)

set (_imiv_platform_glfw_sources
     imiv_drag_drop.cpp
     imiv_platform_glfw.cpp)
if (APPLE)
    list (APPEND _imiv_platform_glfw_sources
          external/dnd_glfw/dnd_glfw_macos.mm)
endif ()

set (_imiv_renderer_vulkan_sources
     imiv_renderer_vulkan.cpp
     imiv_capture.cpp
     imiv_vulkan_setup.cpp
     imiv_vulkan_resource_utils.cpp
     imiv_vulkan_shader_utils.cpp
     imiv_vulkan_ocio.cpp
     imiv_vulkan_preview.cpp
     imiv_vulkan_runtime.cpp
     imiv_vulkan_texture.cpp)

set (_imiv_renderer_metal_sources
     imiv_renderer_metal.mm)
set (_imiv_renderer_opengl_sources
     imiv_renderer_opengl.cpp)
set (_imiv_renderer_enabled_sources)
set (_imiv_core_sources)

set (_imiv_shader_src "${CMAKE_CURRENT_SOURCE_DIR}/shaders/imiv_upload_to_rgba.comp")
set (_imiv_preview_vert_src "${CMAKE_CURRENT_SOURCE_DIR}/shaders/imiv_preview.vert")
set (_imiv_preview_frag_src "${CMAKE_CURRENT_SOURCE_DIR}/shaders/imiv_preview.frag")
set (_imiv_shader_spv_16f "${CMAKE_CURRENT_BINARY_DIR}/imiv_upload_to_rgba16f.comp.spv")
set (_imiv_shader_spv_32f "${CMAKE_CURRENT_BINARY_DIR}/imiv_upload_to_rgba32f.comp.spv")
set (_imiv_shader_spv_16f_fp64 "${CMAKE_CURRENT_BINARY_DIR}/imiv_upload_to_rgba16f_fp64.comp.spv")
set (_imiv_shader_spv_32f_fp64 "${CMAKE_CURRENT_BINARY_DIR}/imiv_upload_to_rgba32f_fp64.comp.spv")
set (_imiv_preview_vert_spv "${CMAKE_CURRENT_BINARY_DIR}/imiv_preview.vert.spv")
set (_imiv_preview_frag_spv "${CMAKE_CURRENT_BINARY_DIR}/imiv_preview.frag.spv")
set (_imiv_shader_hdr_16f "${CMAKE_CURRENT_BINARY_DIR}/imiv_upload_to_rgba16f_spv.h")
set (_imiv_shader_hdr_32f "${CMAKE_CURRENT_BINARY_DIR}/imiv_upload_to_rgba32f_spv.h")
set (_imiv_shader_hdr_16f_fp64 "${CMAKE_CURRENT_BINARY_DIR}/imiv_upload_to_rgba16f_fp64_spv.h")
set (_imiv_shader_hdr_32f_fp64 "${CMAKE_CURRENT_BINARY_DIR}/imiv_upload_to_rgba32f_fp64_spv.h")
set (_imiv_preview_vert_hdr "${CMAKE_CURRENT_BINARY_DIR}/imiv_preview_vert_spv.h")
set (_imiv_preview_frag_hdr "${CMAKE_CURRENT_BINARY_DIR}/imiv_preview_frag_spv.h")
set (_imiv_shader_outputs)
set (_imiv_embedded_shader_headers)
set (_imiv_font_ui_ttf "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans/DroidSans.ttf")
set (_imiv_font_mono_ttf "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans_Mono/DroidSansMono.ttf")
set (_imiv_font_ui_hdr "${CMAKE_CURRENT_BINARY_DIR}/imiv_font_droidsans_ttf.h")
set (_imiv_font_mono_hdr "${CMAKE_CURRENT_BINARY_DIR}/imiv_font_droidsansmono_ttf.h")
set (_imiv_embedded_font_headers)

function (_imiv_add_embedded_spirv_header input_spv output_hdr symbol_name)
    add_custom_command (
        OUTPUT "${output_hdr}"
        COMMAND ${CMAKE_COMMAND}
                -DINPUT="${input_spv}"
                -DOUTPUT="${output_hdr}"
                -DSYMBOL_NAME="${symbol_name}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv_header.cmake"
        DEPENDS
            "${input_spv}"
            "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv_header.cmake"
        COMMENT "imiv: embedding Vulkan shader ${symbol_name}")
endfunction ()

function (_imiv_add_embedded_binary_header input_bin output_hdr symbol_name)
    add_custom_command (
        OUTPUT "${output_hdr}"
        COMMAND ${CMAKE_COMMAND}
                -DINPUT="${input_bin}"
                -DOUTPUT="${output_hdr}"
                -DSYMBOL_NAME="${symbol_name}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_binary_header.cmake"
        DEPENDS
            "${input_bin}"
            "${CMAKE_CURRENT_SOURCE_DIR}/embed_binary_header.cmake"
        COMMENT "imiv: embedding binary asset ${symbol_name}")
endfunction ()

if (OIIO_IMIV_EMBED_FONTS)
    if (NOT EXISTS "${_imiv_font_ui_ttf}" OR NOT EXISTS "${_imiv_font_mono_ttf}")
        message (FATAL_ERROR
                 "imiv: OIIO_IMIV_EMBED_FONTS=ON requires ${_imiv_font_ui_ttf} and ${_imiv_font_mono_ttf}")
    endif ()
    _imiv_add_embedded_binary_header ("${_imiv_font_ui_ttf}"
                                      "${_imiv_font_ui_hdr}"
                                      "g_imiv_font_droidsans_ttf")
    _imiv_add_embedded_binary_header ("${_imiv_font_mono_ttf}"
                                      "${_imiv_font_mono_hdr}"
                                      "g_imiv_font_droidsansmono_ttf")
    list (APPEND _imiv_embedded_font_headers
          "${_imiv_font_ui_hdr}"
          "${_imiv_font_mono_hdr}")
endif ()

if (_imiv_want_vulkan)
    find_program (OIIO_IMIV_GLSLC_EXECUTABLE
                  NAMES glslc
                  HINTS
                      "${OIIO_IMIV_VULKAN_SDK}/bin")
    if (OIIO_IMIV_GLSLC_EXECUTABLE AND EXISTS "${_imiv_shader_src}")
        add_custom_command (
            OUTPUT "${_imiv_shader_spv_16f}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
            COMMAND ${OIIO_IMIV_GLSLC_EXECUTABLE}
                    -fshader-stage=comp
                    -DIMIV_OUTPUT_16F=1
                    -DIMIV_ENABLE_FP64=0
                    -O
                    "${_imiv_shader_src}"
                    -o "${_imiv_shader_spv_16f}"
            DEPENDS "${_imiv_shader_src}"
            COMMENT "imiv: compiling Vulkan compute shader (rgba16f)")
        add_custom_command (
            OUTPUT "${_imiv_shader_spv_32f}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
            COMMAND ${OIIO_IMIV_GLSLC_EXECUTABLE}
                    -fshader-stage=comp
                    -DIMIV_OUTPUT_16F=0
                    -DIMIV_ENABLE_FP64=0
                    -O
                    "${_imiv_shader_src}"
                    -o "${_imiv_shader_spv_32f}"
            DEPENDS "${_imiv_shader_src}"
            COMMENT "imiv: compiling Vulkan compute shader (rgba32f)")
        add_custom_command (
            OUTPUT "${_imiv_shader_spv_16f_fp64}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
            COMMAND ${OIIO_IMIV_GLSLC_EXECUTABLE}
                    -fshader-stage=comp
                    -DIMIV_OUTPUT_16F=1
                    -DIMIV_ENABLE_FP64=1
                    -O
                    "${_imiv_shader_src}"
                    -o "${_imiv_shader_spv_16f_fp64}"
            DEPENDS "${_imiv_shader_src}"
            COMMENT "imiv: compiling Vulkan compute shader (rgba16f, fp64)")
        add_custom_command (
            OUTPUT "${_imiv_shader_spv_32f_fp64}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
            COMMAND ${OIIO_IMIV_GLSLC_EXECUTABLE}
                    -fshader-stage=comp
                    -DIMIV_OUTPUT_16F=0
                    -DIMIV_ENABLE_FP64=1
                    -O
                    "${_imiv_shader_src}"
                    -o "${_imiv_shader_spv_32f_fp64}"
            DEPENDS "${_imiv_shader_src}"
            COMMENT "imiv: compiling Vulkan compute shader (rgba32f, fp64)")
        add_custom_command (
            OUTPUT "${_imiv_preview_vert_spv}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
            COMMAND ${OIIO_IMIV_GLSLC_EXECUTABLE}
                    -fshader-stage=vert
                    -O
                    "${_imiv_preview_vert_src}"
                    -o "${_imiv_preview_vert_spv}"
            DEPENDS "${_imiv_preview_vert_src}"
            COMMENT "imiv: compiling Vulkan preview vertex shader")
        add_custom_command (
            OUTPUT "${_imiv_preview_frag_spv}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
            COMMAND ${OIIO_IMIV_GLSLC_EXECUTABLE}
                    -fshader-stage=frag
                    -O
                    "${_imiv_preview_frag_src}"
                    -o "${_imiv_preview_frag_spv}"
            DEPENDS "${_imiv_preview_frag_src}"
            COMMENT "imiv: compiling Vulkan preview fragment shader")
        _imiv_add_embedded_spirv_header (
            "${_imiv_shader_spv_16f}"
            "${_imiv_shader_hdr_16f}"
            "g_imiv_upload_to_rgba16f_spv")
        _imiv_add_embedded_spirv_header (
            "${_imiv_shader_spv_32f}"
            "${_imiv_shader_hdr_32f}"
            "g_imiv_upload_to_rgba32f_spv")
        _imiv_add_embedded_spirv_header (
            "${_imiv_shader_spv_16f_fp64}"
            "${_imiv_shader_hdr_16f_fp64}"
            "g_imiv_upload_to_rgba16f_fp64_spv")
        _imiv_add_embedded_spirv_header (
            "${_imiv_shader_spv_32f_fp64}"
            "${_imiv_shader_hdr_32f_fp64}"
            "g_imiv_upload_to_rgba32f_fp64_spv")
        _imiv_add_embedded_spirv_header (
            "${_imiv_preview_vert_spv}"
            "${_imiv_preview_vert_hdr}"
            "g_imiv_preview_vert_spv")
        _imiv_add_embedded_spirv_header (
            "${_imiv_preview_frag_spv}"
            "${_imiv_preview_frag_hdr}"
            "g_imiv_preview_frag_spv")
        set (_imiv_shader_outputs
             "${_imiv_shader_spv_16f}"
             "${_imiv_shader_spv_32f}"
             "${_imiv_shader_spv_16f_fp64}"
             "${_imiv_shader_spv_32f_fp64}"
             "${_imiv_preview_vert_spv}"
             "${_imiv_preview_frag_spv}")
        set (_imiv_embedded_shader_headers
             "${_imiv_shader_hdr_16f}"
             "${_imiv_shader_hdr_32f}"
             "${_imiv_shader_hdr_16f_fp64}"
             "${_imiv_shader_hdr_32f_fp64}"
             "${_imiv_preview_vert_hdr}"
             "${_imiv_preview_frag_hdr}")
        add_custom_target (imiv_shaders DEPENDS
                           ${_imiv_shader_outputs}
                           ${_imiv_embedded_shader_headers})
    else ()
        message (STATUS
                 "imiv: glslc not found; Vulkan compute upload shader generation disabled")
    endif ()
endif ()

set (_imiv_imgui_sources
     "${OIIO_IMIV_IMGUI_ROOT}/imgui.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_demo.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_draw.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_tables.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_widgets.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/misc/cpp/imgui_stdlib.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_glfw.cpp"
     ${_imiv_imgui_renderer_sources})

set (_imiv_test_engine_sources)
set (_imiv_test_engine_dir "")
if (OIIO_IMIV_ENABLE_IMGUI_TEST_ENGINE)
    if (OIIO_IMIV_TEST_ENGINE_ROOT)
        if (EXISTS "${OIIO_IMIV_TEST_ENGINE_ROOT}/imgui_te_engine.h")
            set (_imiv_test_engine_dir "${OIIO_IMIV_TEST_ENGINE_ROOT}")
        elseif (EXISTS
                "${OIIO_IMIV_TEST_ENGINE_ROOT}/imgui_test_engine/imgui_te_engine.h")
            set (_imiv_test_engine_dir "${OIIO_IMIV_TEST_ENGINE_ROOT}/imgui_test_engine")
        endif ()
    elseif (DEFINED ENV{IMGUI_TEST_ENGINE_ROOT}
            AND NOT "$ENV{IMGUI_TEST_ENGINE_ROOT}" STREQUAL "")
        if (EXISTS "$ENV{IMGUI_TEST_ENGINE_ROOT}/imgui_te_engine.h")
            set (_imiv_test_engine_dir "$ENV{IMGUI_TEST_ENGINE_ROOT}")
        elseif (EXISTS
                "$ENV{IMGUI_TEST_ENGINE_ROOT}/imgui_test_engine/imgui_te_engine.h")
            set (_imiv_test_engine_dir "$ENV{IMGUI_TEST_ENGINE_ROOT}/imgui_test_engine")
        endif ()
    endif ()

    if (_imiv_test_engine_dir)
        set (_imiv_test_engine_sources
             "${_imiv_test_engine_dir}/imgui_te_context.cpp"
             "${_imiv_test_engine_dir}/imgui_te_coroutine.cpp"
             "${_imiv_test_engine_dir}/imgui_te_engine.cpp"
             "${_imiv_test_engine_dir}/imgui_te_exporters.cpp"
             "${_imiv_test_engine_dir}/imgui_te_perftool.cpp"
             "${_imiv_test_engine_dir}/imgui_te_ui.cpp"
             "${_imiv_test_engine_dir}/imgui_te_utils.cpp"
             "${_imiv_test_engine_dir}/imgui_capture_tool.cpp")
        record_build_dependency (ImGuiTestEngine FOUND)
    else ()
        record_build_dependency (
            ImGuiTestEngine NOTFOUND
            NOT_FOUND_EXPLANATION
                "(sources not found under ${OIIO_IMIV_TEST_ENGINE_ROOT})")
        message (STATUS
                 "imiv: Dear ImGui Test Engine not found; test automation support will be disabled")
    endif ()
else ()
    record_build_dependency (
        ImGuiTestEngine NOTFOUND
        NOT_FOUND_EXPLANATION
            "(disabled by OIIO_IMIV_ENABLE_IMGUI_TEST_ENGINE=OFF)")
endif ()
