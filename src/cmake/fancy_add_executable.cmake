# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Macro to add an executable build target. The executable name can be
# specified with NAME, otherwise is inferred from the subdirectory name. The
# source files of the binary can be specified with SRC, otherwise are
# inferred to be all the .cpp files within the subdirectory. Optional
# compile DEFINITIONS, private INCLUDE_DIRS, and private LINK_LIBRARIES may
# also be specified.
#
# The executable may be disabled individually using any of the usual
# check_is_enabled() conventions (e.g. -DENABLE_<executable>=OFF).
#
# If -DENABLE_INSTALL_<executable>=OFF was specified, the target will be built
# (in the build area) but will not be installed.
#
# Usage:
#
#   fancy_add_executable ([ NAME targetname ... ]
#                         [ SRC source1 ... ]
#                         [ INCLUDE_DIRS include_dir1 ... ]
#                         [ SYSTEM_INCLUDE_DIRS include_dir1 ... ]
#                         [ DEFINITIONS FOO=bar ... ])
#                         [ COMPILE_OPTIONS -Wno-foo ... ]
#                         [ LINK_LIBRARIES external_lib1 ... ]
#                         [ FOLDER foldername ]
#
macro (fancy_add_executable)
    cmake_parse_arguments (_target "NO_INSTALL" "NAME;FOLDER" "SRC;INCLUDE_DIRS;SYSTEM_INCLUDE_DIRS;LINK_LIBRARIES;DEFINITIONS;COMPILE_OPTIONS" ${ARGN})
       # Arguments: <prefix> <options> <one_value_keywords> <multi_value_keywords> args...
    if (NOT _target_NAME)
        # If NAME is not supplied, infer target name (and therefore the
        # executable name) from the directory name.
        get_filename_component (_target_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    endif ()
    if (NOT _target_FOLDER)
        set (_target_FOLDER "Tools")
    endif ()
    if (NOT _target_SRC)
        # If SRC is not supplied, assume local cpp files are its source.
        file (GLOB _target_SRC *.cpp)
    endif ()
    check_is_enabled (${_target_NAME} _target_NAME_enabled)
    if (_target_NAME_enabled)
        add_executable (${_target_NAME} ${_target_SRC})
        target_include_directories (${_target_NAME} PRIVATE ${_target_INCLUDE_DIRS})
        target_include_directories (${_target_NAME} SYSTEM PRIVATE ${_target_SYSTEM_INCLUDE_DIRS})
        target_compile_definitions (${_target_NAME} PRIVATE
                                        ${_target_DEFINITIONS})
        target_compile_options (${_target_NAME} PRIVATE
                                    ${_target_COMPILE_OPTIONS})
        target_link_libraries (${_target_NAME} PRIVATE ${_target_LINK_LIBRARIES})
        target_link_libraries (${_target_NAME} PRIVATE ${PROFILER_LIBRARIES})
        set_target_properties (${_target_NAME} PROPERTIES FOLDER ${_target_FOLDER})
        check_is_enabled (INSTALL_${_target_NAME} _target_NAME_INSTALL_enabled)
        if (CMAKE_UNITY_BUILD AND UNITY_BUILD_MODE STREQUAL GROUP)
            set_source_files_properties(${_target_SRC} PROPERTIES
                                        UNITY_GROUP ${_target_NAME})
            message (VERBOSE "Unity group ${_target_NAME} = ${_target_SRC}")
        endif ()
        if (_target_NAME_INSTALL_enabled AND NOT _target_NO_INSTALL)
            install_targets (${_target_NAME})
        endif ()
    else ()
        message (STATUS "${ColorRed}Disabling ${_target_NAME} ${ColorReset}")
    endif ()
endmacro ()
