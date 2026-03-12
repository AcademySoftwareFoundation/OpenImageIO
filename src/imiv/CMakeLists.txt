# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set (OIIO_IMIV_IMGUI_ROOT "${PROJECT_SOURCE_DIR}/../imgui" CACHE PATH
     "Path to Dear ImGui repository checkout used by imiv")
set (OIIO_IMIV_TEST_ENGINE_ROOT "${PROJECT_SOURCE_DIR}/../imgui_test_engine" CACHE PATH
     "Path to Dear ImGui Test Engine repository checkout used by imiv tests")
set (OIIO_IMIV_NFD_ROOT "$ENV{OIIO_IMIV_NFD_ROOT}" CACHE PATH
     "Path hint for nativefiledialog-extended source/build/install root used by imiv")
set (OIIO_IMIV_NFD_INCLUDE_DIR "" CACHE PATH
     "Optional explicit include directory containing nfd.h")
set (OIIO_IMIV_NFD_LIBRARY_RELEASE "" CACHE FILEPATH
     "Optional explicit nativefiledialog release library path")
set (OIIO_IMIV_NFD_LIBRARY_DEBUG "" CACHE FILEPATH
     "Optional explicit nativefiledialog debug library path")
set (OIIO_IMIV_GLSLANG_INCLUDE_DIR "" CACHE PATH
     "Optional explicit include directory containing glslang headers for imiv runtime shader compilation")
set (OIIO_IMIV_GLSLANG_LIBRARY "" CACHE FILEPATH
     "Optional explicit glslang library path for imiv runtime shader compilation")
set (OIIO_IMIV_GLSLANG_DEFAULT_LIMITS_LIBRARY "" CACHE FILEPATH
     "Optional explicit glslang-default-resource-limits library path for imiv runtime shader compilation")
option (OIIO_IMIV_USE_NATIVEFILEDIALOG
        "Use nativefiledialog-extended for imiv file open/save actions" ON)
option (OIIO_IMIV_ENABLE_IMGUI_TEST_ENGINE
        "Build imiv with Dear ImGui Test Engine integration when sources are available" ON)
option (OIIO_IMIV_ADD_UPLOAD_SMOKE_CTEST
        "Add an optional CTest entry that generates/upload-tests the imiv corpus" OFF)
set (OIIO_IMIV_VULKAN_SDK "$ENV{VULKAN_SDK}" CACHE PATH
     "Optional Vulkan SDK root path used to locate Vulkan package for imiv")

check_is_enabled (imiv imiv_enabled)
if (NOT imiv_enabled)
    message (STATUS "Disabling imiv")
    return ()
endif ()

set (_imiv_imgui_required_files
     "${OIIO_IMIV_IMGUI_ROOT}/imgui.h"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_draw.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_tables.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_widgets.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_glfw.h"
     "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_glfw.cpp")
if (NOT APPLE)
    list (APPEND _imiv_imgui_required_files
          "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_vulkan.h"
          "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_vulkan.cpp")
endif ()
set (_imiv_imgui_found ON)
foreach (_imiv_required_file ${_imiv_imgui_required_files})
    if (NOT EXISTS "${_imiv_required_file}")
        set (_imiv_imgui_found OFF)
        break ()
    endif ()
endforeach ()

if (NOT _imiv_imgui_found)
    record_build_dependency (
        ImGui NOTFOUND
        NOT_FOUND_EXPLANATION
            "(sources not found under ${OIIO_IMIV_IMGUI_ROOT})")
    message (STATUS
             "\n\n   WARNING: Dear ImGui not found at '${OIIO_IMIV_IMGUI_ROOT}' -- 'imiv' will not be built!\n")
    return ()
endif ()
record_build_dependency (ImGui FOUND)

set (_imiv_core_sources
     imiv_actions.cpp
     imiv_app.cpp
     imiv_aux_windows.cpp
     imiv_capture.cpp
     imiv_drag_drop.cpp
     imiv_file_dialog.cpp
     imiv_frame.cpp
     imiv_image_view.cpp
     imiv_menu.cpp
     imiv_navigation.cpp
     imiv_ocio.cpp
     imiv_overlays.cpp
     imiv_shader_compile.cpp
     imiv_ui.cpp
     imiv_viewer.cpp
     imiv_vulkan_setup.cpp
     imiv_vulkan_ocio.cpp
     imiv_vulkan_preview.cpp
     imiv_vulkan_runtime.cpp
     imiv_vulkan_texture.cpp
     imiv_vulkan_window.cpp
     imiv_test_engine.cpp
     imiv_main.cpp)

if (APPLE)
    list (APPEND _imiv_core_sources
          external/dnd_glfw/dnd_glfw_macos.mm)
endif ()

set (_imiv_shader_src "${CMAKE_CURRENT_SOURCE_DIR}/shaders/imiv_upload_to_rgba.comp")
set (_imiv_preview_vert_src "${CMAKE_CURRENT_SOURCE_DIR}/shaders/imiv_preview.vert")
set (_imiv_preview_frag_src "${CMAKE_CURRENT_SOURCE_DIR}/shaders/imiv_preview.frag")
set (_imiv_shader_spv_16f "${CMAKE_CURRENT_BINARY_DIR}/imiv_upload_to_rgba16f.comp.spv")
set (_imiv_shader_spv_32f "${CMAKE_CURRENT_BINARY_DIR}/imiv_upload_to_rgba32f.comp.spv")
set (_imiv_shader_spv_16f_fp64 "${CMAKE_CURRENT_BINARY_DIR}/imiv_upload_to_rgba16f_fp64.comp.spv")
set (_imiv_shader_spv_32f_fp64 "${CMAKE_CURRENT_BINARY_DIR}/imiv_upload_to_rgba32f_fp64.comp.spv")
set (_imiv_preview_vert_spv "${CMAKE_CURRENT_BINARY_DIR}/imiv_preview.vert.spv")
set (_imiv_preview_frag_spv "${CMAKE_CURRENT_BINARY_DIR}/imiv_preview.frag.spv")
set (_imiv_shader_outputs)

if (NOT APPLE)
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
        set (_imiv_shader_outputs
             "${_imiv_shader_spv_16f}"
             "${_imiv_shader_spv_32f}"
             "${_imiv_shader_spv_16f_fp64}"
             "${_imiv_shader_spv_32f_fp64}"
             "${_imiv_preview_vert_spv}"
             "${_imiv_preview_frag_spv}")
        add_custom_target (imiv_shaders DEPENDS ${_imiv_shader_outputs})
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
     "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_glfw.cpp")
if (NOT APPLE)
    list (APPEND _imiv_imgui_sources
          "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_vulkan.cpp")
endif ()

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

set (_imiv_link_libs
     OpenImageIO
     $<TARGET_NAME_IF_EXISTS:OpenColorIO::OpenColorIO>
     $<TARGET_NAME_IF_EXISTS:OpenColorIO::OpenColorIOHeaders>)
if (NOT APPLE)
    find_package (glfw3 CONFIG QUIET)
    if (TARGET glfw)
        list (APPEND _imiv_link_libs glfw)
    else ()
        message (STATUS
                 "\n\n   WARNING: glfw not found -- 'imiv' will not be built!\n")
        return ()
    endif ()

    find_package (Vulkan QUIET)
    if (Vulkan_FOUND)
        record_build_dependency (Vulkan FOUND VERSION "${Vulkan_VERSION}")
        list (APPEND _imiv_link_libs Vulkan::Vulkan)
    else ()
        find_path (OIIO_IMIV_VULKAN_INCLUDE_DIR
                   NAMES vulkan/vulkan.h
                   HINTS
                       "${OIIO_IMIV_VULKAN_SDK}/include")
        find_library (OIIO_IMIV_VULKAN_LIBRARY
                      NAMES vulkan libvulkan.so
                      HINTS
                          "${OIIO_IMIV_VULKAN_SDK}/lib")
        if (OIIO_IMIV_VULKAN_INCLUDE_DIR AND OIIO_IMIV_VULKAN_LIBRARY)
            record_build_dependency (Vulkan FOUND)
            add_library (imiv_vulkan_external UNKNOWN IMPORTED)
            set_target_properties (
                imiv_vulkan_external PROPERTIES
                    IMPORTED_LOCATION "${OIIO_IMIV_VULKAN_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES
                    "${OIIO_IMIV_VULKAN_INCLUDE_DIR}")
            list (APPEND _imiv_link_libs imiv_vulkan_external)
            message (STATUS "imiv: using Vulkan from explicit SDK include/library paths")
        else ()
            record_build_dependency (
                Vulkan NOTFOUND
                NOT_FOUND_EXPLANATION
                    "(required by imiv on this platform)")
            message (STATUS
                     "\n\n   WARNING: Vulkan not found -- 'imiv' will not be built!\n")
            return ()
        endif ()
    endif ()

    set (_imiv_has_runtime_glslang OFF)
    set (_imiv_runtime_glslang_explanation "")
    set (_imiv_glslang_search_hints)
    set (_imiv_glslang_package_roots)
    foreach (_imiv_prefix IN LISTS CMAKE_PREFIX_PATH)
        if (_imiv_prefix)
            list (APPEND _imiv_glslang_package_roots "${_imiv_prefix}")
            list (APPEND _imiv_glslang_search_hints
                  "${_imiv_prefix}"
                  "${_imiv_prefix}/include"
                  "${_imiv_prefix}/Include"
                  "${_imiv_prefix}/lib"
                  "${_imiv_prefix}/Lib"
                  "${_imiv_prefix}/lib64")
        endif ()
    endforeach ()
    set (_imiv_windows_static_crt OFF)
    if (WIN32 AND MSVC)
        string (TOUPPER "${CMAKE_BUILD_TYPE}" _imiv_build_type_upper)
        set (_imiv_msvc_runtime_flags
             "${CMAKE_MSVC_RUNTIME_LIBRARY} ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${_imiv_build_type_upper}}")
        if (_imiv_msvc_runtime_flags MATCHES "MultiThreaded(Debug)?"
            OR _imiv_msvc_runtime_flags MATCHES "(^|[ /;])MTd?($|[ /;])")
            set (_imiv_windows_static_crt ON)
        endif ()
    endif ()
    if (OIIO_IMIV_VULKAN_SDK)
        list (APPEND _imiv_glslang_package_roots "${OIIO_IMIV_VULKAN_SDK}")
        list (APPEND _imiv_glslang_search_hints
              "${OIIO_IMIV_VULKAN_SDK}/include"
              "${OIIO_IMIV_VULKAN_SDK}/Include"
              "${OIIO_IMIV_VULKAN_SDK}/lib"
              "${OIIO_IMIV_VULKAN_SDK}/Lib"
              "${OIIO_IMIV_VULKAN_SDK}/lib64")
    endif ()

    set (_imiv_glslang_include_dir "${OIIO_IMIV_GLSLANG_INCLUDE_DIR}")
    set (_imiv_glslang_library "${OIIO_IMIV_GLSLANG_LIBRARY}")
    set (_imiv_glslang_default_limits_library
         "${OIIO_IMIV_GLSLANG_DEFAULT_LIMITS_LIBRARY}")
    set (_imiv_glslang_manual_override OFF)
    if (_imiv_glslang_include_dir
        AND _imiv_glslang_library
        AND _imiv_glslang_default_limits_library)
        set (_imiv_glslang_manual_override ON)
    endif ()

    set (_imiv_glslang_package_found OFF)
    if (NOT _imiv_glslang_manual_override)
        find_package (glslang CONFIG QUIET
                      PATHS ${_imiv_glslang_package_roots})
        if (glslang_FOUND
            AND TARGET glslang::glslang
            AND TARGET glslang::glslang-default-resource-limits)
            set (_imiv_glslang_package_found ON)
        endif ()
    endif ()

    set (_imiv_glslang_from_vulkan_sdk OFF)
    if (_imiv_glslang_package_found AND OIIO_IMIV_VULKAN_SDK)
        file (TO_CMAKE_PATH "${OIIO_IMIV_VULKAN_SDK}" _imiv_vulkan_sdk_path)
        file (TO_CMAKE_PATH "${glslang_DIR}" _imiv_glslang_dir_norm)
        string (FIND "${_imiv_glslang_dir_norm}" "${_imiv_vulkan_sdk_path}"
                     _imiv_glslang_dir_pos)
        if (_imiv_glslang_dir_pos EQUAL 0)
            set (_imiv_glslang_from_vulkan_sdk ON)
        endif ()
    elseif (OIIO_IMIV_VULKAN_SDK
        AND _imiv_glslang_include_dir
        AND _imiv_glslang_library
        AND _imiv_glslang_default_limits_library)
        file (TO_CMAKE_PATH "${OIIO_IMIV_VULKAN_SDK}" _imiv_vulkan_sdk_path)
        set (_imiv_glslang_from_vulkan_sdk ON)
        foreach (_imiv_glslang_path
                 "${_imiv_glslang_include_dir}"
                 "${_imiv_glslang_library}"
                 "${_imiv_glslang_default_limits_library}")
            file (TO_CMAKE_PATH "${_imiv_glslang_path}" _imiv_glslang_path_norm)
            string (FIND "${_imiv_glslang_path_norm}" "${_imiv_vulkan_sdk_path}"
                         _imiv_glslang_path_pos)
            if (NOT _imiv_glslang_path_pos EQUAL 0)
                set (_imiv_glslang_from_vulkan_sdk OFF)
                break ()
            endif ()
        endforeach ()
    endif ()

    if (WIN32 AND _imiv_windows_static_crt AND _imiv_glslang_from_vulkan_sdk)
        set (_imiv_runtime_glslang_explanation
             "(runtime shader compilation disabled; Vulkan SDK glslang libraries use /MD and are incompatible with this /MT build; use /MD or provide custom glslang libraries built with the same MSVC runtime via CMAKE_PREFIX_PATH or OIIO_IMIV_GLSLANG_* paths)")
    elseif (_imiv_glslang_package_found)
        list (APPEND _imiv_link_libs
              glslang::glslang
              glslang::glslang-default-resource-limits)
        set (_imiv_has_runtime_glslang ON)
        message (STATUS "imiv: using glslang package from ${glslang_DIR}")
        if (DEFINED glslang_VERSION)
            record_build_dependency (GLSLANG_RUNTIME FOUND VERSION "${glslang_VERSION}")
        else ()
            record_build_dependency (GLSLANG_RUNTIME FOUND)
        endif ()
    elseif (_imiv_glslang_include_dir
            AND _imiv_glslang_library
            AND _imiv_glslang_default_limits_library)
        add_library (imiv_glslang_external UNKNOWN IMPORTED)
        set_target_properties (
            imiv_glslang_external PROPERTIES
                IMPORTED_LOCATION "${_imiv_glslang_library}"
                INTERFACE_INCLUDE_DIRECTORIES
                    "${_imiv_glslang_include_dir}")
        add_library (imiv_glslang_resource_limits_external UNKNOWN IMPORTED)
        set_target_properties (
            imiv_glslang_resource_limits_external PROPERTIES
                IMPORTED_LOCATION
                    "${_imiv_glslang_default_limits_library}"
                INTERFACE_INCLUDE_DIRECTORIES
                    "${_imiv_glslang_include_dir}")
        list (APPEND _imiv_link_libs
              imiv_glslang_external
              imiv_glslang_resource_limits_external)
        set (_imiv_has_runtime_glslang ON)
        message (STATUS "imiv: using glslang from explicit include/library paths")
        record_build_dependency (GLSLANG_RUNTIME FOUND)
    else ()
        find_path (_imiv_glslang_include_dir
                   NAMES glslang/Include/glslang_c_interface.h
                   HINTS ${_imiv_glslang_search_hints}
                   PATH_SUFFIXES include Include
                   NO_CACHE)
        set (_imiv_glslang_saved_library_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")
        if (WIN32)
            set (CMAKE_FIND_LIBRARY_SUFFIXES .lib .dll.a)
        elseif (APPLE)
            set (CMAKE_FIND_LIBRARY_SUFFIXES .dylib .so)
        else ()
            set (CMAKE_FIND_LIBRARY_SUFFIXES .so)
        endif ()
        find_library (_imiv_glslang_library
                      NAMES glslang glslangd libglslang
                      HINTS ${_imiv_glslang_search_hints}
                      PATH_SUFFIXES lib Lib lib64
                      NO_CACHE)
        find_library (_imiv_glslang_default_limits_library
                      NAMES glslang-default-resource-limits
                            glslang-default-resource-limitsd
                            libglslang-default-resource-limits
                      HINTS ${_imiv_glslang_search_hints}
                      PATH_SUFFIXES lib Lib lib64
                      NO_CACHE)
        set (CMAKE_FIND_LIBRARY_SUFFIXES "${_imiv_glslang_saved_library_suffixes}")
        if (_imiv_glslang_include_dir
            AND _imiv_glslang_library
            AND _imiv_glslang_default_limits_library)
            add_library (imiv_glslang_external UNKNOWN IMPORTED)
            set_target_properties (
                imiv_glslang_external PROPERTIES
                    IMPORTED_LOCATION "${_imiv_glslang_library}"
                    INTERFACE_INCLUDE_DIRECTORIES
                        "${_imiv_glslang_include_dir}")
            add_library (imiv_glslang_resource_limits_external UNKNOWN IMPORTED)
            set_target_properties (
                imiv_glslang_resource_limits_external PROPERTIES
                    IMPORTED_LOCATION
                        "${_imiv_glslang_default_limits_library}"
                    INTERFACE_INCLUDE_DIRECTORIES
                        "${_imiv_glslang_include_dir}")
            list (APPEND _imiv_link_libs
                  imiv_glslang_external
                  imiv_glslang_resource_limits_external)
            set (_imiv_has_runtime_glslang ON)
            message (STATUS "imiv: using glslang discovered from include/library search paths")
            record_build_dependency (GLSLANG_RUNTIME FOUND)
        endif ()
    endif ()

    if (NOT _imiv_has_runtime_glslang)
        if (NOT _imiv_glslang_include_dir)
            list (APPEND _imiv_runtime_glslang_missing
                  "glslang/Include/glslang_c_interface.h")
        endif ()
        if (NOT _imiv_glslang_library)
            list (APPEND _imiv_runtime_glslang_missing "glslang library")
        endif ()
        if (NOT _imiv_glslang_default_limits_library)
            list (APPEND _imiv_runtime_glslang_missing
                  "glslang-default-resource-limits library")
        endif ()
        string (REPLACE ";" ", " _imiv_runtime_glslang_missing_text
                "${_imiv_runtime_glslang_missing}")
        if (_imiv_runtime_glslang_explanation STREQUAL "")
            if (CMAKE_PREFIX_PATH)
                string (REPLACE ";" ", " _imiv_glslang_prefix_path_text
                        "${CMAKE_PREFIX_PATH}")
                set (_imiv_runtime_glslang_explanation
                     "(runtime shader compilation disabled; missing ${_imiv_runtime_glslang_missing_text}; searched CMAKE_PREFIX_PATH=${_imiv_glslang_prefix_path_text}, Vulkan SDK hints, and default paths)")
            elseif (OIIO_IMIV_VULKAN_SDK)
                set (_imiv_runtime_glslang_explanation
                     "(runtime shader compilation disabled; missing ${_imiv_runtime_glslang_missing_text}; searched ${OIIO_IMIV_VULKAN_SDK} and default paths)")
            else ()
                set (_imiv_runtime_glslang_explanation
                     "(runtime shader compilation disabled; missing ${_imiv_runtime_glslang_missing_text}; set CMAKE_PREFIX_PATH, OIIO_IMIV_VULKAN_SDK, or OIIO_IMIV_GLSLANG_* paths)")
            endif ()
        endif ()
    endif ()
    if (NOT _imiv_has_runtime_glslang)
        record_build_dependency (
            GLSLANG_RUNTIME NOTFOUND
            NOT_FOUND_EXPLANATION "${_imiv_runtime_glslang_explanation}")
    endif ()
else ()
    set (_imiv_has_runtime_glslang OFF)
    record_build_dependency (
        GLSLANG_RUNTIME NOTFOUND
        NOT_FOUND_EXPLANATION
            "(runtime shader compilation disabled on Metal backend)")
endif ()

if (MSVC)
    set_source_files_properties (${_imiv_imgui_sources}
                                 PROPERTIES COMPILE_OPTIONS "/w")
else ()
    set_source_files_properties (${_imiv_imgui_sources}
                                 PROPERTIES COMPILE_OPTIONS "-w")
endif ()

fancy_add_executable (
    NAME imiv
    SRC
        ${_imiv_core_sources}
        ${_imiv_imgui_sources}
        ${_imiv_test_engine_sources}
    INCLUDE_DIRS
        "${OIIO_IMIV_IMGUI_ROOT}"
        "${OIIO_IMIV_IMGUI_ROOT}/backends"
        "${_imiv_test_engine_dir}"
    LINK_LIBRARIES
        ${_imiv_link_libs}
)

if (TARGET imiv)
    target_compile_definitions (imiv PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
    target_compile_definitions (imiv PRIVATE IMIV_SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    if (_imiv_shader_outputs)
        target_compile_definitions (imiv PRIVATE IMIV_HAS_COMPUTE_UPLOAD_SHADERS=1)
        add_dependencies (imiv imiv_shaders)
    else ()
        target_compile_definitions (imiv PRIVATE IMIV_HAS_COMPUTE_UPLOAD_SHADERS=0)
    endif ()
    if (APPLE)
        target_compile_definitions (imiv PRIVATE IMIV_BACKEND_METAL_GLFW=1)
        find_library (OIIO_IMIV_COCOA_FRAMEWORK Cocoa)
        if (OIIO_IMIV_COCOA_FRAMEWORK)
            target_link_libraries (imiv PRIVATE ${OIIO_IMIV_COCOA_FRAMEWORK})
        endif ()
    else ()
        target_compile_definitions (imiv PRIVATE IMIV_BACKEND_VULKAN_GLFW=1
                                                  $<$<CONFIG:Debug>:IMIV_VULKAN_VALIDATION=1>)
    endif ()
    if (_imiv_has_runtime_glslang)
        target_compile_definitions (imiv PRIVATE IMIV_HAS_GLSLANG_RUNTIME=1)
    else ()
        target_compile_definitions (imiv PRIVATE IMIV_HAS_GLSLANG_RUNTIME=0)
    endif ()
    if (_imiv_test_engine_sources)
        target_compile_definitions (
            imiv PRIVATE
                IMGUI_ENABLE_TEST_ENGINE
                IMGUI_TEST_ENGINE_ENABLE_IMPLOT=0
                IMGUI_TEST_ENGINE_ENABLE_CAPTURE=1
                IMGUI_TEST_ENGINE_ENABLE_STD_FUNCTION=0
                IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1)
    endif ()

    if (OIIO_IMIV_USE_NATIVEFILEDIALOG)
        if (OIIO_IMIV_NFD_ROOT)
            find_package (nfd CONFIG QUIET
                          PATHS
                              "${OIIO_IMIV_NFD_ROOT}"
                              "${OIIO_IMIV_NFD_ROOT}/build"
                              "${OIIO_IMIV_NFD_ROOT}/install"
                              "${OIIO_IMIV_NFD_ROOT}/dist")
        else ()
            find_package (nfd CONFIG QUIET)
        endif ()
        if (TARGET nfd::nfd)
            target_link_libraries (imiv PRIVATE nfd::nfd)
            target_compile_definitions (imiv PRIVATE IMIV_HAS_NFD=1)
            message (STATUS "imiv: using nativefiledialog target nfd::nfd")
        else ()
            if (NOT OIIO_IMIV_NFD_INCLUDE_DIR)
                find_path (OIIO_IMIV_NFD_INCLUDE_DIR nfd.h
                           HINTS
                               "${OIIO_IMIV_NFD_ROOT}/src/include")
            endif ()
            if (NOT OIIO_IMIV_NFD_LIBRARY_RELEASE)
                find_library (OIIO_IMIV_NFD_LIBRARY_RELEASE
                              NAMES nfd nfd_static
                              HINTS
                                  "${OIIO_IMIV_NFD_ROOT}"
                                  "${OIIO_IMIV_NFD_ROOT}/build"
                                  "${OIIO_IMIV_NFD_ROOT}/lib"
                                  "${OIIO_IMIV_NFD_ROOT}/dist/lib")
            endif ()
            if (OIIO_IMIV_NFD_INCLUDE_DIR AND OIIO_IMIV_NFD_LIBRARY_RELEASE)
                add_library (imiv_nfd_external UNKNOWN IMPORTED)
                set_target_properties (imiv_nfd_external PROPERTIES
                    IMPORTED_LOCATION "${OIIO_IMIV_NFD_LIBRARY_RELEASE}"
                    IMPORTED_LOCATION_RELEASE "${OIIO_IMIV_NFD_LIBRARY_RELEASE}"
                    INTERFACE_INCLUDE_DIRECTORIES "${OIIO_IMIV_NFD_INCLUDE_DIR}")
                if (OIIO_IMIV_NFD_LIBRARY_DEBUG)
                    set_target_properties (imiv_nfd_external PROPERTIES
                        IMPORTED_LOCATION_DEBUG "${OIIO_IMIV_NFD_LIBRARY_DEBUG}")
                endif ()
                target_link_libraries (imiv PRIVATE imiv_nfd_external)
                target_compile_definitions (imiv PRIVATE IMIV_HAS_NFD=1)
                message (STATUS "imiv: using nativefiledialog from explicit include/library paths")
            else ()
                target_compile_definitions (imiv PRIVATE IMIV_HAS_NFD=0)
                message (STATUS
                         "imiv: nativefiledialog requested but not found yet; file dialogs will stay disabled")
            endif ()
        endif ()
    else ()
        target_compile_definitions (imiv PRIVATE IMIV_HAS_NFD=0)
    endif ()

    add_custom_command (
        TARGET imiv POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:imiv>/fonts/Droid_Sans"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:imiv>/fonts/Droid_Sans_Mono"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans/DroidSans.ttf"
                "$<TARGET_FILE_DIR:imiv>/fonts/Droid_Sans/DroidSans.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans/DroidSans-Bold.ttf"
                "$<TARGET_FILE_DIR:imiv>/fonts/Droid_Sans/DroidSans-Bold.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans/droid-lic.txt"
                "$<TARGET_FILE_DIR:imiv>/fonts/Droid_Sans/droid-lic.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans_Mono/DroidSansMono.ttf"
                "$<TARGET_FILE_DIR:imiv>/fonts/Droid_Sans_Mono/DroidSansMono.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans_Mono/droid-lic.txt"
                "$<TARGET_FILE_DIR:imiv>/fonts/Droid_Sans_Mono/droid-lic.txt"
        VERBATIM)

    install (FILES
                 "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans/DroidSans.ttf"
                 "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans/DroidSans-Bold.ttf"
                 "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans/droid-lic.txt"
             DESTINATION
                 "${CMAKE_INSTALL_BINDIR}/fonts/Droid_Sans")
    install (FILES
                 "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans_Mono/DroidSansMono.ttf"
                 "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans_Mono/droid-lic.txt"
             DESTINATION
                 "${CMAKE_INSTALL_BINDIR}/fonts/Droid_Sans_Mono")
endif ()

if (MSVC)
    set_source_files_properties (${_imiv_test_engine_sources}
                                 PROPERTIES COMPILE_OPTIONS "/w")
else ()
    set_source_files_properties (${_imiv_test_engine_sources}
                                 PROPERTIES COMPILE_OPTIONS "-w")
endif ()

if (TARGET imiv
        AND OIIO_BUILD_TESTS
        AND BUILD_TESTING
        AND OIIO_IMIV_ADD_UPLOAD_SMOKE_CTEST
        AND UNIX)
    find_program (OIIO_IMIV_BASH_EXECUTABLE NAMES bash)
    if (OIIO_IMIV_BASH_EXECUTABLE)
        add_test (
            NAME imiv_upload_smoke
            COMMAND
                "${OIIO_IMIV_BASH_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_upload_corpus_ctest.sh"
                "${CMAKE_BINARY_DIR}")
        set_tests_properties (
            imiv_upload_smoke PROPERTIES
                LABELS "imiv;imiv_upload_smoke;gui"
                TIMEOUT 1800)
    else ()
        message (STATUS
                 "imiv: OIIO_IMIV_ADD_UPLOAD_SMOKE_CTEST requested but no bash executable was found")
    endif ()
endif ()

set (_imiv_ocio_live_update_config
     "${PROJECT_SOURCE_DIR}/temp/studio-config-all-views-v4.0.0_aces-v2.0_ocio-v2.5.ocio")
if (TARGET imiv
        AND OIIO_BUILD_TESTS
        AND BUILD_TESTING
        AND Python3_EXECUTABLE
        AND _imiv_test_engine_sources
        AND _imiv_has_runtime_glslang
        AND EXISTS "${_imiv_ocio_live_update_config}")
    add_test (
        NAME imiv_ocio_live_update_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_ocio_live_update_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            --oiiotool "$<TARGET_FILE:oiiotool>"
            --idiff "$<TARGET_FILE:idiff>"
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/ocio_live_update_regression"
            --image "${CMAKE_BINARY_DIR}/imiv_captures/ocio_live_update_regression/ocio_live_input.exr"
            --ocio-config "${_imiv_ocio_live_update_config}")
    set_tests_properties (
        imiv_ocio_live_update_regression PROPERTIES
            LABELS "imiv;imiv_ocio;gui"
            TIMEOUT 300)

    add_test (
        NAME imiv_ocio_live_display_update_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_ocio_live_update_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            --oiiotool "$<TARGET_FILE:oiiotool>"
            --idiff "$<TARGET_FILE:idiff>"
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/ocio_live_display_update_regression"
            --image "${CMAKE_BINARY_DIR}/imiv_captures/ocio_live_display_update_regression/ocio_live_input.exr"
            --ocio-config "${_imiv_ocio_live_update_config}"
            --display "sRGB - Display"
            --target-display "Display P3 - Display"
            --raw-view "Un-tone-mapped"
            --target-view "Un-tone-mapped")
    set_tests_properties (
        imiv_ocio_live_display_update_regression PROPERTIES
            LABELS "imiv;imiv_ocio;gui"
            TIMEOUT 300)
elseif (TARGET imiv
        AND OIIO_BUILD_TESTS
        AND BUILD_TESTING)
    message (STATUS
             "imiv: live OCIO update regression test not added (requires Python3, ImGui Test Engine, runtime glslang, and temp OCIO config)")
endif ()

if (TARGET imiv
        AND OIIO_BUILD_TESTS
        AND BUILD_TESTING
        AND Python3_EXECUTABLE
        AND _imiv_test_engine_sources)
    add_test (
        NAME imiv_developer_menu_regression
        COMMAND
            "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/imiv_developer_menu_regression.py"
            --bin "$<TARGET_FILE:imiv>"
            --cwd "$<TARGET_FILE_DIR:imiv>"
            --env-script "${CMAKE_BINARY_DIR}/imiv_env.sh"
            --out-dir "${CMAKE_BINARY_DIR}/imiv_captures/developer_menu_regression"
            --open "${PROJECT_SOURCE_DIR}/ASWF/logos/openimageio-stacked-gradient.png")
    set_tests_properties (
        imiv_developer_menu_regression PROPERTIES
            LABELS "imiv;imiv_debug;gui"
            TIMEOUT 120
            SKIP_RETURN_CODE 77)
elseif (TARGET imiv
        AND OIIO_BUILD_TESTS
        AND BUILD_TESTING)
    message (STATUS
             "imiv: developer menu regression test not added (requires Python3 and ImGui Test Engine)")
endif ()
