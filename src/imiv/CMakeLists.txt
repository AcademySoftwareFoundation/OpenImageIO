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
option (OIIO_IMIV_EMBED_FONTS
        "Embed imiv UI and mono fonts into the binary" ON)
option (OIIO_IMIV_ENABLE_IMGUI_TEST_ENGINE
        "Build imiv with Dear ImGui Test Engine integration when sources are available" ON)
option (OIIO_IMIV_ADD_UPLOAD_SMOKE_CTEST
        "Add an optional CTest entry that generates/upload-tests the imiv corpus" OFF)
option (OIIO_IMIV_ADD_BACKEND_VERIFY_CTEST
        "Add per-backend shared verification CTest entries for each compiled imiv backend" OFF)
set (OIIO_IMIV_VULKAN_SDK "$ENV{VULKAN_SDK}" CACHE PATH
     "Optional Vulkan SDK root path used to locate Vulkan package for imiv")
set (_imiv_default_renderer "vulkan")
if (APPLE)
    set (_imiv_default_renderer "metal")
endif ()
set (OIIO_IMIV_RENDERER "" CACHE STRING
     "Deprecated alias for OIIO_IMIV_DEFAULT_RENDERER")
set_property (CACHE OIIO_IMIV_RENDERER PROPERTY STRINGS
              "" auto vulkan metal opengl)
set (OIIO_IMIV_DEFAULT_RENDERER "auto" CACHE STRING
     "Default renderer backend for imiv (auto, vulkan, metal, opengl)")
set_property (CACHE OIIO_IMIV_DEFAULT_RENDERER PROPERTY STRINGS
              auto vulkan metal opengl)
set (OIIO_IMIV_ENABLE_VULKAN "AUTO" CACHE STRING
     "Enable Vulkan backend for imiv (AUTO, ON, OFF)")
set (OIIO_IMIV_ENABLE_METAL "AUTO" CACHE STRING
     "Enable Metal backend for imiv (AUTO, ON, OFF)")
set (OIIO_IMIV_ENABLE_OPENGL "AUTO" CACHE STRING
     "Enable OpenGL backend for imiv (AUTO, ON, OFF)")
set_property (CACHE OIIO_IMIV_ENABLE_VULKAN PROPERTY STRINGS AUTO ON OFF)
set_property (CACHE OIIO_IMIV_ENABLE_METAL PROPERTY STRINGS AUTO ON OFF)
set_property (CACHE OIIO_IMIV_ENABLE_OPENGL PROPERTY STRINGS AUTO ON OFF)

check_is_enabled (imiv imiv_enabled)
if (NOT imiv_enabled)
    message (STATUS "Disabling imiv")
    return ()
endif ()

string (TOLOWER "${OIIO_IMIV_RENDERER}" _imiv_renderer_compat_request)
string (TOLOWER "${OIIO_IMIV_DEFAULT_RENDERER}" _imiv_default_renderer_request)
if (NOT _imiv_renderer_compat_request STREQUAL "")
    if (_imiv_default_renderer_request STREQUAL ""
        OR _imiv_default_renderer_request STREQUAL "auto") 
        set (_imiv_default_renderer_request "${_imiv_renderer_compat_request}")
        message (STATUS
                 "imiv: OIIO_IMIV_RENDERER is deprecated; treating it as OIIO_IMIV_DEFAULT_RENDERER")
    endif ()
endif ()
if (_imiv_default_renderer_request STREQUAL "")
    set (_imiv_default_renderer_request "auto")
endif ()
if (NOT (_imiv_default_renderer_request STREQUAL "auto"
         OR _imiv_default_renderer_request STREQUAL "vulkan"
         OR _imiv_default_renderer_request STREQUAL "metal"
         OR _imiv_default_renderer_request STREQUAL "opengl"))
    message (FATAL_ERROR
             "imiv: unsupported OIIO_IMIV_DEFAULT_RENDERER='${OIIO_IMIV_DEFAULT_RENDERER}' "
             "(expected auto, vulkan, metal, or opengl)")
endif ()

string (TOUPPER "${OIIO_IMIV_ENABLE_VULKAN}" _imiv_enable_vulkan_mode)
string (TOUPPER "${OIIO_IMIV_ENABLE_METAL}" _imiv_enable_metal_mode)
string (TOUPPER "${OIIO_IMIV_ENABLE_OPENGL}" _imiv_enable_opengl_mode)
foreach (_imiv_mode_var
         _imiv_enable_vulkan_mode
         _imiv_enable_metal_mode
         _imiv_enable_opengl_mode)
    if (NOT ("${${_imiv_mode_var}}" STREQUAL "AUTO"
             OR "${${_imiv_mode_var}}" STREQUAL "ON"
             OR "${${_imiv_mode_var}}" STREQUAL "OFF"))
        message (FATAL_ERROR
                 "imiv: ${_imiv_mode_var} must be AUTO, ON, or OFF")
    endif ()
endforeach ()

set (_imiv_want_vulkan OFF)
set (_imiv_want_metal OFF)
set (_imiv_want_opengl OFF)
if (NOT _imiv_enable_vulkan_mode STREQUAL "OFF")
    set (_imiv_want_vulkan ON)
endif ()
if (NOT _imiv_enable_metal_mode STREQUAL "OFF")
    set (_imiv_want_metal ON)
endif ()
if (NOT _imiv_enable_opengl_mode STREQUAL "OFF")
    set (_imiv_want_opengl ON)
endif ()
if (_imiv_enable_metal_mode STREQUAL "ON" AND NOT APPLE)
    message (FATAL_ERROR
             "imiv: OIIO_IMIV_ENABLE_METAL=ON requires APPLE")
endif ()
if (NOT APPLE AND _imiv_enable_metal_mode STREQUAL "AUTO")
    set (_imiv_want_metal OFF)
endif ()
set (_imiv_enabled_vulkan OFF)
set (_imiv_enabled_metal OFF)
set (_imiv_enabled_opengl OFF)

set (_imiv_imgui_required_files
     "${OIIO_IMIV_IMGUI_ROOT}/imgui.h"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_draw.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_tables.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_widgets.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_glfw.h"
     "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_glfw.cpp")
set (_imiv_imgui_renderer_sources)
if (_imiv_want_vulkan)
    list (APPEND _imiv_imgui_required_files
          "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_vulkan.h"
          "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_vulkan.cpp")
endif ()
if (_imiv_want_metal)
    list (APPEND _imiv_imgui_required_files
          "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_metal.h"
          "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui_impl_metal_imiv.mm")
endif ()
if (_imiv_want_opengl)
    list (APPEND _imiv_imgui_required_files
          "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_opengl3.h"
          "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_opengl3.cpp")
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

include ("${CMAKE_CURRENT_LIST_DIR}/cmake/imiv_sources.cmake")

set (_imiv_link_libs
     OpenImageIO
     $<TARGET_NAME_IF_EXISTS:OpenColorIO::OpenColorIO>
     $<TARGET_NAME_IF_EXISTS:OpenColorIO::OpenColorIOHeaders>)
find_package (glfw3 CONFIG QUIET)
if (TARGET glfw)
    list (APPEND _imiv_link_libs glfw)
else ()
    message (STATUS
             "\n\n   WARNING: glfw not found -- 'imiv' will not be built!\n")
    return ()
endif ()

set (_imiv_has_runtime_glslang OFF)
if (_imiv_want_vulkan)
    find_package (Vulkan QUIET)
    if (Vulkan_FOUND)
        list (APPEND _imiv_link_libs Vulkan::Vulkan)
        set (_imiv_enabled_vulkan ON)
        record_build_dependency (Vulkan FOUND VERSION "${Vulkan_VERSION}")
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
            add_library (imiv_vulkan_external UNKNOWN IMPORTED)
            set_target_properties (
                imiv_vulkan_external PROPERTIES
                    IMPORTED_LOCATION "${OIIO_IMIV_VULKAN_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES
                    "${OIIO_IMIV_VULKAN_INCLUDE_DIR}")
            list (APPEND _imiv_link_libs imiv_vulkan_external)
            set (_imiv_enabled_vulkan ON)
            record_build_dependency (Vulkan FOUND)
            message (STATUS "imiv: using Vulkan from explicit SDK include/library paths")
        elseif (_imiv_enable_vulkan_mode STREQUAL "ON")
            message (FATAL_ERROR
                     "imiv: Vulkan backend was requested but Vulkan was not found")
        else ()
            record_build_dependency (
                Vulkan NOTFOUND
                NOT_FOUND_EXPLANATION
                    "(Vulkan backend not enabled; dependency discovery failed in AUTO mode)")
        endif ()
    endif ()

    if (_imiv_enabled_vulkan)
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
            record_build_dependency (
                GLSLANG_RUNTIME NOTFOUND
                NOT_FOUND_EXPLANATION "${_imiv_runtime_glslang_explanation}")
        endif ()
    else ()
        record_build_dependency (
            GLSLANG_RUNTIME NOTFOUND
            NOT_FOUND_EXPLANATION
                "(runtime shader compilation not checked because Vulkan backend is disabled)")
    endif ()
else ()
    record_build_dependency (
        Vulkan NOTFOUND
        NOT_FOUND_EXPLANATION
            "(disabled by OIIO_IMIV_ENABLE_VULKAN=OFF)")
    record_build_dependency (
        GLSLANG_RUNTIME NOTFOUND
        NOT_FOUND_EXPLANATION
            "(runtime shader compilation disabled because Vulkan backend is off)")
endif ()

if (_imiv_want_metal)
    find_library (OIIO_IMIV_COCOA_FRAMEWORK Cocoa)
    find_library (OIIO_IMIV_METAL_FRAMEWORK Metal)
    find_library (OIIO_IMIV_QUARTZCORE_FRAMEWORK QuartzCore)
    if (OIIO_IMIV_COCOA_FRAMEWORK
        AND OIIO_IMIV_METAL_FRAMEWORK
        AND OIIO_IMIV_QUARTZCORE_FRAMEWORK)
        list (APPEND _imiv_link_libs
              ${OIIO_IMIV_COCOA_FRAMEWORK}
              ${OIIO_IMIV_METAL_FRAMEWORK}
              ${OIIO_IMIV_QUARTZCORE_FRAMEWORK})
        set (_imiv_enabled_metal ON)
        record_build_dependency (Metal FOUND)
    elseif (_imiv_enable_metal_mode STREQUAL "ON")
        message (FATAL_ERROR
                 "imiv: Metal backend was requested but required frameworks were not found")
    endif ()
endif ()

if (_imiv_want_opengl)
    find_package (OpenGL QUIET)
    if (TARGET OpenGL::GL)
        list (APPEND _imiv_link_libs OpenGL::GL)
        set (_imiv_enabled_opengl ON)
        record_build_dependency (OpenGL FOUND)
    elseif (TARGET OpenGL::OpenGL)
        list (APPEND _imiv_link_libs OpenGL::OpenGL)
        set (_imiv_enabled_opengl ON)
        record_build_dependency (OpenGL FOUND)
    elseif (_imiv_enable_opengl_mode STREQUAL "ON")
        message (FATAL_ERROR
                 "imiv: OpenGL backend was requested but OpenGL was not found")
    endif ()
endif ()

if (NOT _imiv_enabled_vulkan
    AND NOT _imiv_enabled_metal
    AND NOT _imiv_enabled_opengl)
    message (STATUS
             "\n\n   WARNING: no renderer backends were enabled -- 'imiv' will not be built!\n")
    return ()
endif ()

set (_imiv_selected_renderer "")
if (_imiv_default_renderer_request STREQUAL "auto")
    if (APPLE)
        if (_imiv_enabled_metal)
            set (_imiv_selected_renderer "metal")
        elseif (_imiv_enabled_vulkan)
            set (_imiv_selected_renderer "vulkan")
        elseif (_imiv_enabled_opengl)
            set (_imiv_selected_renderer "opengl")
        endif ()
    else ()
        if (_imiv_enabled_vulkan)
            set (_imiv_selected_renderer "vulkan")
        elseif (_imiv_enabled_opengl)
            set (_imiv_selected_renderer "opengl")
        elseif (_imiv_enabled_metal)
            set (_imiv_selected_renderer "metal")
        endif ()
    endif ()
elseif (_imiv_default_renderer_request STREQUAL "vulkan" AND _imiv_enabled_vulkan)
    set (_imiv_selected_renderer "vulkan")
elseif (_imiv_default_renderer_request STREQUAL "metal" AND _imiv_enabled_metal)
    set (_imiv_selected_renderer "metal")
elseif (_imiv_default_renderer_request STREQUAL "opengl" AND _imiv_enabled_opengl)
    set (_imiv_selected_renderer "opengl")
endif ()
if (_imiv_selected_renderer STREQUAL "")
    if (APPLE)
        if (_imiv_enabled_metal)
            set (_imiv_selected_renderer "metal")
        elseif (_imiv_enabled_vulkan)
            set (_imiv_selected_renderer "vulkan")
        else ()
            set (_imiv_selected_renderer "opengl")
        endif ()
    else ()
        if (_imiv_enabled_vulkan)
            set (_imiv_selected_renderer "vulkan")
        elseif (_imiv_enabled_opengl)
            set (_imiv_selected_renderer "opengl")
        else ()
            set (_imiv_selected_renderer "metal")
        endif ()
    endif ()
    if (NOT _imiv_default_renderer_request STREQUAL "auto")
        message (STATUS
                 "imiv: requested build default backend '${_imiv_default_renderer_request}' is not enabled; falling back to ${_imiv_selected_renderer}")
    endif ()
endif ()

set (_imiv_enabled_backend_count 0)
if (_imiv_enabled_vulkan)
    math (EXPR _imiv_enabled_backend_count
          "${_imiv_enabled_backend_count} + 1")
endif ()
if (_imiv_enabled_metal)
    math (EXPR _imiv_enabled_backend_count
          "${_imiv_enabled_backend_count} + 1")
endif ()
if (_imiv_enabled_opengl)
    math (EXPR _imiv_enabled_backend_count
          "${_imiv_enabled_backend_count} + 1")
endif ()

set (_imiv_renderer_is_vulkan OFF)
set (_imiv_renderer_is_metal OFF)
set (_imiv_renderer_is_opengl OFF)
if (_imiv_selected_renderer STREQUAL "vulkan")
    set (_imiv_renderer_is_vulkan ON)
elseif (_imiv_selected_renderer STREQUAL "metal")
    set (_imiv_renderer_is_metal ON)
elseif (_imiv_selected_renderer STREQUAL "opengl")
    set (_imiv_renderer_is_opengl ON)
endif ()

set (IMIV_WITH_VULKAN 0)
set (IMIV_WITH_METAL 0)
set (IMIV_WITH_OPENGL 0)
set (IMIV_EMBED_FONTS 0)
set (IMIV_BUILD_DEFAULT_BACKEND_KIND -1)
if (_imiv_enabled_vulkan)
    set (IMIV_WITH_VULKAN 1)
endif ()
if (_imiv_enabled_metal)
    set (IMIV_WITH_METAL 1)
endif ()
if (_imiv_enabled_opengl)
    set (IMIV_WITH_OPENGL 1)
endif ()
if (OIIO_IMIV_EMBED_FONTS)
    set (IMIV_EMBED_FONTS 1)
endif ()
if (_imiv_renderer_is_vulkan)
    set (IMIV_BUILD_DEFAULT_BACKEND_KIND 0)
elseif (_imiv_renderer_is_metal)
    set (IMIV_BUILD_DEFAULT_BACKEND_KIND 1)
elseif (_imiv_renderer_is_opengl)
    set (IMIV_BUILD_DEFAULT_BACKEND_KIND 2)
endif ()

configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/imiv_build_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/imiv_build_config.h"
    @ONLY)

message (STATUS
         "imiv: enabled renderer backends ="
         " vulkan=${_imiv_enabled_vulkan}"
         " metal=${_imiv_enabled_metal}"
         " opengl=${_imiv_enabled_opengl}")
message (STATUS
         "imiv: build default renderer backend = glfw+${_imiv_selected_renderer}")

set (_imiv_renderer_enabled_sources)
if (_imiv_enabled_vulkan)
    list (APPEND _imiv_renderer_enabled_sources ${_imiv_renderer_vulkan_sources})
endif ()
if (_imiv_enabled_metal)
    list (APPEND _imiv_renderer_enabled_sources ${_imiv_renderer_metal_sources})
endif ()
if (_imiv_enabled_opengl)
    list (APPEND _imiv_renderer_enabled_sources ${_imiv_renderer_opengl_sources})
endif ()
set (_imiv_core_sources
     ${_imiv_shared_sources}
     ${_imiv_test_engine_integration_sources}
     ${_imiv_platform_glfw_sources}
     ${_imiv_renderer_enabled_sources}
     ${_imiv_embedded_font_headers})

set (_imiv_imgui_renderer_sources)
if (_imiv_enabled_vulkan)
    list (APPEND _imiv_imgui_renderer_sources
          "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_vulkan.cpp")
endif ()
if (_imiv_enabled_metal)
    list (APPEND _imiv_imgui_renderer_sources
          "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui_impl_metal_imiv.mm")
endif ()
if (_imiv_enabled_opengl)
    list (APPEND _imiv_imgui_renderer_sources
          "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_opengl3.cpp")
endif ()

set (_imiv_imgui_sources
     "${OIIO_IMIV_IMGUI_ROOT}/imgui.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_demo.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_draw.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_tables.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/imgui_widgets.cpp"
     "${OIIO_IMIV_IMGUI_ROOT}/backends/imgui_impl_glfw.cpp"
     ${_imiv_imgui_renderer_sources})

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
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${OIIO_IMIV_IMGUI_ROOT}"
        "${OIIO_IMIV_IMGUI_ROOT}/backends"
        "${_imiv_test_engine_dir}"
    LINK_LIBRARIES
        ${_imiv_link_libs}
)

source_group ("imgui" FILES ${_imiv_imgui_sources})
source_group ("imgui_te" FILES
              ${_imiv_test_engine_sources}
              ${_imiv_test_engine_integration_sources}
              "${CMAKE_CURRENT_SOURCE_DIR}/imiv_test_engine.h")
source_group ("Source Files" FILES ${_imiv_shared_sources})
source_group ("Source Files\\platform\\glfw" FILES ${_imiv_platform_glfw_sources})
source_group ("Source Files\\renderer\\vulkan" FILES ${_imiv_renderer_vulkan_sources})
if (_imiv_renderer_metal_sources)
    source_group ("Source Files\\renderer\\metal" FILES ${_imiv_renderer_metal_sources})
endif ()
if (_imiv_renderer_opengl_sources)
    source_group ("Source Files\\renderer\\opengl" FILES ${_imiv_renderer_opengl_sources})
endif ()

if (TARGET imiv)
    target_compile_definitions (imiv PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
    target_compile_definitions (imiv PRIVATE IMIV_SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    if (_imiv_shader_outputs)
        target_compile_definitions (imiv PRIVATE IMIV_HAS_COMPUTE_UPLOAD_SHADERS=1)
        add_dependencies (imiv imiv_shaders)
    else ()
        target_compile_definitions (imiv PRIVATE IMIV_HAS_COMPUTE_UPLOAD_SHADERS=0)
    endif ()
    if (_imiv_embedded_shader_headers)
        target_compile_definitions (imiv PRIVATE IMIV_HAS_EMBEDDED_VULKAN_SHADERS=1)
    else ()
        target_compile_definitions (imiv PRIVATE IMIV_HAS_EMBEDDED_VULKAN_SHADERS=0)
    endif ()
    if (_imiv_renderer_is_vulkan)
        target_compile_definitions (imiv PRIVATE IMIV_BACKEND_VULKAN_GLFW=1
                                                  $<$<CONFIG:Debug>:IMIV_VULKAN_VALIDATION=1>)
    elseif (_imiv_renderer_is_metal)
        target_compile_definitions (imiv PRIVATE IMIV_BACKEND_METAL_GLFW=1)
    elseif (_imiv_renderer_is_opengl)
        target_compile_definitions (imiv PRIVATE IMIV_BACKEND_OPENGL_GLFW=1)
    endif ()
    if (APPLE AND _imiv_enabled_opengl)
        target_compile_definitions (imiv PRIVATE GL_SILENCE_DEPRECATION)
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

    if (NOT OIIO_IMIV_EMBED_FONTS)
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
                     "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans/droid-lic.txt"
                 DESTINATION
                     "${CMAKE_INSTALL_BINDIR}/fonts/Droid_Sans")
        install (FILES
                     "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans_Mono/DroidSansMono.ttf"
                     "${PROJECT_SOURCE_DIR}/src/fonts/Droid_Sans_Mono/droid-lic.txt"
                 DESTINATION
                     "${CMAKE_INSTALL_BINDIR}/fonts/Droid_Sans_Mono")
    endif ()
endif ()

if (MSVC)
    set_source_files_properties (${_imiv_test_engine_sources}
                                 PROPERTIES COMPILE_OPTIONS "/w")
else ()
    set_source_files_properties (${_imiv_test_engine_sources}
                                 PROPERTIES COMPILE_OPTIONS "-w")
endif ()

include ("${CMAKE_CURRENT_LIST_DIR}/cmake/imiv_tests.cmake")
