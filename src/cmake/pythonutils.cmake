# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

# Python-related options.
option (USE_PYTHON "Build the Python bindings" ON)
set (PYTHON_VERSION "2.7" CACHE STRING "Target version of python to find")
option (PYLIB_INCLUDE_SONAME "If ON, soname/soversion will be set for Python module library" OFF)
option (PYLIB_LIB_PREFIX "If ON, prefix the Python module with 'lib'" OFF)
set (PYMODULE_SUFFIX "" CACHE STRING "Suffix to add to Python module init namespace")


# Find Python. This macro should only be called if python is required. If
# Python cannot be found, it will be a fatal error.
# Variables set by this macro:
#    PYTHON_INCLUDES_PATH - directory where python headers are found
#    PYTHON_LIBRARIES     - python libraries to link
#    PYTHON_SITE_DIR      - our own install dir where our python modules go
macro (find_python)
    if (NOT VERBOSE)
        set (PythonInterp_FIND_QUIETLY true)
        set (PythonLibs_FIND_QUIETLY true)
    endif ()

    # Attempt to find the desired version, but fall back to other
    # additional versions.
    checked_find_package (PythonInterp ${PYTHON_VERSION} REQUIRED)

    # The version that was found may not be the default or user
    # defined one.
    set (PYTHON_VERSION_FOUND ${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR})

    if (NOT ${PYTHON_VERSION} VERSION_EQUAL ${PYTHON_VERSION_FOUND} )
        message (WARNING "The requested version ${PYTHON_VERSION} was not found.") 
        message (WARNING "Using ${PYTHON_VERSION_FOUND} instead.")
    endif ()

    checked_find_package (PythonLibs ${PYTHON_VERSION_FOUND} REQUIRED)
    if (VERBOSE)
        message (STATUS "    Python include dirs ${PYTHON_INCLUDE_PATH}")
        message (STATUS "    Python libraries    ${PYTHON_LIBRARIES}")
        message (STATUS "    Python site packages dir ${PYTHON_SITE_DIR}")
        message (STATUS "    Python to include 'lib' prefix: ${PYLIB_LIB_PREFIX}")
        message (STATUS "    Python to include SO version: ${PYLIB_INCLUDE_SONAME}")
        message (STATUS "    Python version ${PYTHON_VERSION_STRING}")
        message (STATUS "    Python version major: ${PYTHON_VERSION_MAJOR} minor: ${PYTHON_VERSION_MINOR}")
    endif ()

    if (NOT DEFINED PYTHON_SITE_DIR)
        set (PYTHON_SITE_DIR "${CMAKE_INSTALL_LIBDIR}/python${PYTHON_VERSION_FOUND}/site-packages")
    endif ()
endmacro()


###########################################################################
# pybind11

macro (setup_python_module)
    cmake_parse_arguments (lib "" "TARGET;MODULE;LIBS" "SOURCES" ${ARGN})
    # Arguments: <prefix> <options> <one_value_keywords> <multi_value_keywords> args...

    set (target_name ${lib_TARGET})

    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        # Seems to be a problem on some systems, with pybind11 and python headers
        set_property (SOURCE ${lib_SOURCES} APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-macro-redefined ")
    endif ()

    # Add the library itself
    add_library (${target_name} MODULE ${lib_SOURCES})

    # Set up include dirs for python & pybind11
    target_include_directories (${target_name} SYSTEM PRIVATE ${PYTHON_INCLUDE_PATH})

    # Declare the libraries it should link against
    target_link_libraries (${target_name} PRIVATE
                           pybind11::pybind11 ${lib_LIBS} ${SANITIZE_LIBRARIES})
    if (APPLE)
        set_target_properties (${target_name} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
    else ()
        target_link_libraries (${target_name} PRIVATE ${PYTHON_LIBRARIES})
    endif ()

    # Exclude the 'lib' prefix from the name
    if (NOT PYLIB_LIB_PREFIX)
        target_compile_definitions(${target_name}
                                   PRIVATE "PYMODULE_NAME=${lib_MODULE}")
        set_target_properties (${target_name} PROPERTIES
                               OUTPUT_NAME ${lib_MODULE}
                               PREFIX "")
    else ()
        target_compile_definitions(${target_name}
                                   PRIVATE "PYMODULE_NAME=Py${lib_MODULE}")
        set_target_properties (${target_name} PROPERTIES
                               OUTPUT_NAME Py${lib_MODULE}
                               PREFIX lib)
    endif ()

    if (PYLIB_INCLUDE_SONAME)
        if (VERBOSE)
            message(STATUS "Setting Py${lib_MODULE} SOVERSION to: ${SOVERSION}")
        endif ()
        set_target_properties(${target_name} PROPERTIES
            VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
            SOVERSION ${SOVERSION} )
    endif()

    if (WIN32)
        set_target_properties (${target_name} PROPERTIES
                               DEBUG_POSTFIX "_d"
                               SUFFIX ".pyd")
    endif()

    # In the build area, put it in lib/python so it doesn't clash with the
    # non-python libraries of the same name (which aren't prefixed by "lib"
    # on Windows).
    set_target_properties (${target_name} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/python/site-packages
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/python/site-packages
            )

    install (TARGETS ${target_name}
             RUNTIME DESTINATION ${PYTHON_SITE_DIR} COMPONENT user
             LIBRARY DESTINATION ${PYTHON_SITE_DIR} COMPONENT user)

endmacro ()

