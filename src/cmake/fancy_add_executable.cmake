# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

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
# Usage:
#
#   fancy_add_executable ([ NAME targetname ... ]
#                         [ SRC source1 ... ]
#                         [ INCLUDE_DIRS include_dir1 ... ]
#                         [ DEFINITIONS -DFOO=bar ... ])
#                         [ LINK_LIBRARIES external_lib1 ... ]
#
macro (fancy_add_executable)
    cmake_parse_arguments (_target "" "NAME" "SRC;INCLUDE_DIRS;SYSTEM_INCLUDE_DIRS;LINK_LIBRARIES;DEFINITIONS" ${ARGN})
       # Arguments: <prefix> <options> <one_value_keywords> <multi_value_keywords> args...
    if (NOT _target_NAME)
        # If NAME is not supplied, infer target name (and therefore the
        # executable name) from the directory name.
        get_filename_component (_target_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    endif ()
    if (NOT _target_SRC)
        # If SRC is not supplied, assume local cpp files are its source.
        file (GLOB _target_SRC *.cpp)
    endif ()
    check_is_enabled (${_target_NAME} _target_NAME_enabled)
    if (_target_NAME_enabled)
        add_executable (${_target_NAME} ${_target_SRC})
        if (_target_INCLUDE_DIRS)
            target_include_directories (${_target_NAME} PRIVATE ${_target_INCLUDE_DIRS})
        endif ()
        if (_target_SYSTEM_INCLUDE_DIRS)
            target_include_directories (${_target_NAME} SYSTEM PRIVATE ${_target_SYSTEM_INCLUDE_DIRS})
        endif ()
        if (_target_DEFINITIONS)
            target_compile_definitions (${_target_name} PRIVATE ${_target_DEFINITIONS})
        endif ()
        if (_target_LINK_LIBRARIES)
            target_link_libraries (${_target_NAME} PRIVATE ${_target_LINK_LIBRARIES})
        endif ()
        set_target_properties (${_target_NAME} PROPERTIES FOLDER "Tools")
        install_targets (${_target_NAME})
    else ()
        message (STATUS "${ColorRed}Disabling ${_target_NAME} ${ColorReset}")
    endif ()
endmacro ()
