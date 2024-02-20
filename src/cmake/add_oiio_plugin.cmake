# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Macro to add a build target for an IO plugin.
#
# Usage:
#
# add_oiio_plugin ( source1 [source2 ...]
#                   [ NAME targetname ... ]
#                   [ SRC source1 ... ]
#                   [ INCLUDE_DIRS include_dir1 ... ]
#                   [ LINK_LIBRARIES external_lib1 ... ]
#                   [ DEFINITIONS -DFOO=bar ... ])
#
# The plugin name can be specified with NAME, otherwise is inferred from the
# subdirectory name. The source files of the binary can be specified with
# SRC, otherwise are inferred to be all the .cpp files within the
# subdirectory. Optional compile DEFINITIONS, private INCLUDE_DIRS, and
# private LINK_LIBRARIES may also be specified. The source is automatically
# linked against OpenImageIO.
#
# The plugin may be disabled individually using any of the usual
# check_is_enabled() conventions (e.g. -DENABLE_<format>=OFF).
#
# What goes on under the covers is quite different depending on whether
# EMBEDPLUGINS is 0 or 1. If EMBEDPLUGINS is 0 (in which case this is
# expected to be called *after* the OpenImageIO target is declared), it will
# create a new target to build the full plugin. On the other hand, if
# EMBEDPLUGINS is 1 (in which case this should be called *before* the
# OpenImageIO target is declared), it will merely append the required
# definitions, includes, and libraries to lists format_plugin_blah that will
# be handed off too the setup of the later OpenImageIO target.
#
macro (add_oiio_plugin)
    cmake_parse_arguments (_plugin "" "NAME" "SRC;INCLUDE_DIRS;LINK_LIBRARIES;DEFINITIONS" ${ARGN})
       # Arguments: <prefix> <options> <one_value_keywords> <multi_value_keywords> args...
    get_filename_component (_plugin_name ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)
    if (NOT _plugin_NAME)
        # If NAME is not supplied, infer target name (and therefore the
        # library name) from the directory name.
        get_filename_component (_plugin_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    endif ()
    # If SRC is supplied, use it. Otherwise, assume any unparsed arguments are the
    # source files. If there are none, then glob *.cpp of the current directory.
    if (NOT _plugin_SRC)
        set (_plugin_SRC ${_plugin_UNPARSED_ARGUMENTS})
    endif ()
    if (NOT _plugin_SRC)
        file (GLOB _plugin_SRC *.cpp)
    endif ()
    check_is_enabled (${_plugin_name} _enable_plugin)
    if (_enable_plugin)
        if (EMBEDPLUGINS)
            # Add each source file to the libOpenImageIO_srcs, but it takes some
            # bending over backwards to change it in the parent scope.
            set (_plugin_all_source ${libOpenImageIO_srcs})
            foreach (_plugin_source_file ${_plugin_SRC})
                list (APPEND _plugin_all_source "${CMAKE_CURRENT_SOURCE_DIR}/${_plugin_source_file}")
            endforeach ()
            set (libOpenImageIO_srcs "${_plugin_all_source}" PARENT_SCOPE)
            set (format_plugin_definitions ${format_plugin_definitions} ${_plugin_DEFINITIONS} PARENT_SCOPE)
            set (format_plugin_include_dirs ${format_plugin_include_dirs} ${_plugin_INCLUDE_DIRS} PARENT_SCOPE)
            set (format_plugin_libs ${format_plugin_libs} ${_plugin_LINK_LIBRARIES} PARENT_SCOPE)
        else ()
            # # Get the name of the current directory and use it as the target name.
            # get_filename_component (_plugin_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
            add_library (${_plugin_NAME} MODULE ${_plugin_SRC})
            target_compile_definitions (${_plugin_NAME} PRIVATE
                                        ${_plugin_DEFINITIONS}
                                        OpenImageIO_EXPORTS)
            target_include_directories (${_plugin_NAME} PRIVATE ${_plugin_INCLUDE_DIRS})
            target_link_libraries (${_plugin_NAME} PUBLIC OpenImageIO
                                                   PRIVATE ${_plugin_LINK_LIBRARIES})
            set_target_properties (${_plugin_NAME} PROPERTIES PREFIX "" FOLDER "Plugins")
            install_targets (${_plugin_NAME})
        endif ()
    else ()
        message (STATUS "${ColorRed}Disabling ${_plugin_name} ${ColorReset}")
        string (TOUPPER ${_plugin_name} _plugin_name_upper)
        set (format_plugin_definitions ${format_plugin_definitions} DISABLE_${_plugin_name_upper} PARENT_SCOPE)
    endif ()
endmacro ()
