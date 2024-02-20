# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Python-related options.
option (USE_PYTHON "Build the Python bindings" ON)
set (PYTHON_VERSION "" CACHE STRING "Target version of python to find")
option (PYLIB_INCLUDE_SONAME "If ON, soname/soversion will be set for Python module library" OFF)
option (PYLIB_LIB_PREFIX "If ON, prefix the Python module with 'lib'" OFF)
set (PYMODULE_SUFFIX "" CACHE STRING "Suffix to add to Python module init namespace")
if (WIN32)
    set (PYLIB_LIB_TYPE SHARED CACHE STRING "Type of library to build for python module (MODULE or SHARED)")
else ()
    set (PYLIB_LIB_TYPE MODULE CACHE STRING "Type of library to build for python module (MODULE or SHARED)")
endif ()


# Find Python. This macro should only be called if python is required. If
# Python cannot be found, it will be a fatal error.
macro (find_python)
    if (NOT VERBOSE)
        set (PythonInterp_FIND_QUIETLY true)
        set (PythonLibs_FIND_QUIETLY true)
    endif ()

    # Attempt to find the desired version, but fall back to other
    # additional versions.
    unset (_req)
    if (USE_PYTHON)
        set (_req REQUIRED)
        if (PYTHON_VERSION)
            list (APPEND _req EXACT)
        endif ()
    endif ()
    checked_find_package (Python ${PYTHON_VERSION}
                          ${_req}
                          COMPONENTS Interpreter Development
                          PRINT Python_VERSION Python_EXECUTABLE
                                Python_LIBRARIES
                                Python_Development_FOUND
                                Python_Interpreter_FOUND )

    # The version that was found may not be the default or user
    # defined one.
    set (PYTHON_VERSION_FOUND ${Python_VERSION_MAJOR}.${Python_VERSION_MINOR})

    # Give hints to subsequent pybind11 searching to ensure that it finds
    # exactly the same version that we found.
    set (PythonInterp_FIND_VERSION PYTHON_VERSION_FOUND)
    set (PythonInterp_FIND_VERSION_MAJOR ${Python_VERSION_MAJOR})

    if (NOT DEFINED PYTHON_SITE_DIR)
        set (PYTHON_SITE_DIR "${CMAKE_INSTALL_LIBDIR}/python${PYTHON_VERSION_FOUND}/site-packages/OpenImageIO")
    endif ()
    message (VERBOSE "    Python site packages dir ${PYTHON_SITE_DIR}")
    message (VERBOSE "    Python to include 'lib' prefix: ${PYLIB_LIB_PREFIX}")
    message (VERBOSE "    Python to include SO version: ${PYLIB_INCLUDE_SONAME}")
endmacro()


###########################################################################
# pybind11

macro (setup_python_module)
    cmake_parse_arguments (lib "" "TARGET;MODULE" "SOURCES;LIBS" ${ARGN})
    # Arguments: <prefix> <options> <one_value_keywords> <multi_value_keywords> args...

    set (target_name ${lib_TARGET})

    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" AND NOT ${CMAKE_COMPILER_ID} STREQUAL "Intel")
        # Seems to be a problem on some systems, with pybind11 and python headers
        set_property (SOURCE ${lib_SOURCES} APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-macro-redefined ")
    endif ()

    pybind11_add_module(${target_name} ${PYLIB_LIB_TYPE} ${lib_SOURCES})

#    # Add the library itself
#    add_library (${target_name} MODULE ${lib_SOURCES})
#
    # Declare the libraries it should link against
    target_link_libraries (${target_name}
                           PRIVATE ${lib_LIBS})

    set (_module_LINK_FLAGS "${VISIBILITY_MAP_COMMAND} ${EXTRA_DSO_LINK_ARGS}")
    if (UNIX AND NOT APPLE)
        # Hide symbols from any static dependent libraries embedded here.
        set (_module_LINK_FLAGS "${_module_LINK_FLAGS} -Wl,--exclude-libs,ALL")
    endif ()
    set_target_properties (${target_name} PROPERTIES LINK_FLAGS ${_module_LINK_FLAGS})


    # Exclude the 'lib' prefix from the name
    if (NOT PYLIB_LIB_PREFIX)
        target_compile_definitions(${target_name}
                                   PRIVATE "PYMODULE_NAME=${lib_MODULE}")
        set_target_properties (${target_name} PROPERTIES
                               OUTPUT_NAME ${lib_MODULE}
                               # PREFIX ""
                               )
    else ()
        target_compile_definitions(${target_name}
                                   PRIVATE "PYMODULE_NAME=Py${lib_MODULE}")
        set_target_properties (${target_name} PROPERTIES
                               OUTPUT_NAME Py${lib_MODULE}
                               PREFIX lib)
    endif ()

    # Even if using a debug postfix, don't use it for the python module, or
    # python won't find it properly.
    set_target_properties(PyOpenImageIO PROPERTIES
                          DEBUG_POSTFIX "")

    # This is only needed for SpComp2
    if (PYLIB_INCLUDE_SONAME)
        message(VERBOSE "Setting Py${lib_MODULE} SOVERSION to: ${SOVERSION}")
        set_target_properties(${target_name} PROPERTIES
            VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
            SOVERSION ${SOVERSION} )
    endif()

#    if (WIN32)
#        set_target_properties (${target_name} PROPERTIES
#                               DEBUG_POSTFIX "_d"
#                               SUFFIX ".pyd")
#    endif()

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

    install(FILES __init__.py DESTINATION ${PYTHON_SITE_DIR})

endmacro ()

