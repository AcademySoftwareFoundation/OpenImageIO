# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Python-related options.
option (USE_PYTHON "Build the Python bindings" ON)
set (PYTHON_VERSION "" CACHE STRING "Target version of python to find")
option (PYLIB_INCLUDE_SONAME "If ON, soname/soversion will be set for Python module library" OFF)
option (PYLIB_LIB_PREFIX "If ON, prefix the Python module with 'lib'" OFF)
set (PYMODULE_SUFFIX "" CACHE STRING "Suffix to add to Python module init namespace")
set (OIIO_PYTHON_BINDINGS_BACKEND "pybind11" CACHE STRING
     "Which Python binding backend(s) to build: pybind11, nanobind, or both")
set_property (CACHE OIIO_PYTHON_BINDINGS_BACKEND PROPERTY STRINGS
              pybind11 nanobind both)

# Normalize and validate the user-facing backend selector early so the rest
# of the file can make simple boolean decisions.
string (TOLOWER "${OIIO_PYTHON_BINDINGS_BACKEND}" OIIO_PYTHON_BINDINGS_BACKEND)
if (NOT OIIO_PYTHON_BINDINGS_BACKEND MATCHES "^(pybind11|nanobind|both)$")
    message (FATAL_ERROR
             "OIIO_PYTHON_BINDINGS_BACKEND must be one of: pybind11, nanobind, both")
endif ()

# Derive internal switches used by the top-level CMakeLists and the Python
# helper macros below.
set (OIIO_BUILD_PYTHON_PYBIND11 OFF)
set (OIIO_BUILD_PYTHON_NANOBIND OFF)
if (OIIO_PYTHON_BINDINGS_BACKEND STREQUAL "pybind11"
        OR OIIO_PYTHON_BINDINGS_BACKEND STREQUAL "both")
    set (OIIO_BUILD_PYTHON_PYBIND11 ON)
endif ()
if (OIIO_PYTHON_BINDINGS_BACKEND STREQUAL "nanobind"
        OR OIIO_PYTHON_BINDINGS_BACKEND STREQUAL "both")
    set (OIIO_BUILD_PYTHON_NANOBIND ON)
endif ()
if (WIN32)
    set (PYLIB_LIB_TYPE SHARED CACHE STRING "Type of library to build for python module (MODULE or SHARED)")
else ()
    set (PYLIB_LIB_TYPE MODULE CACHE STRING "Type of library to build for python module (MODULE or SHARED)")
endif ()


# Find Python. This macro should only be called if python is required. If
# Python cannot be found, it will be a fatal error.
macro (find_python)
    if (NOT VERBOSE)
        set (PythonInterp3_FIND_QUIETLY true)
        set (PythonLibs3_FIND_QUIETLY true)
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

    # Support building on manylinux docker images, which do not contain
    # the Development.Embedded component.
    # https://pybind11.readthedocs.io/en/stable/compiling.html#findpython-mode
    if (WIN32)
        set (_py_components Interpreter Development)
    else ()
        set (_py_components Interpreter Development.Module)
    endif ()

    checked_find_package (Python3 ${PYTHON_VERSION}
                          ${_req}
                          VERSION_MIN 3.7
                          RECOMMEND_MIN 3.9
                          RECOMMEND_MIN_REASON "We don't test or support older than 3.9"
                          COMPONENTS ${_py_components}
                          PRINT Python3_VERSION Python3_EXECUTABLE
                                Python3_LIBRARIES
                                Python3_Development_FOUND
                                Python3_Development.Module_FOUND
                                Python3_Interpreter_FOUND )

    if (OIIO_BUILD_PYTHON_NANOBIND)
        # nanobind's CMake package expects the generic FindPython targets and
        # variables (Python::Module, Python_EXECUTABLE, etc.), not the
        # versioned Python3::* targets that the rest of OIIO uses today.
        find_package (Python ${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}
                      EXACT REQUIRED
                      COMPONENTS ${_py_components})
    endif ()

    # The version that was found may not be the default or user
    # defined one.
    set (PYTHON_VERSION_FOUND ${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR})

    # Give hints to subsequent pybind11 searching to ensure that it finds
    # exactly the same version that we found.
    set (PythonInterp3_FIND_VERSION PYTHON_VERSION_FOUND)
    set (PythonInterp3_FIND_VERSION_MAJOR ${Python3_VERSION_MAJOR})

    if (NOT DEFINED PYTHON_SITE_ROOT_DIR)
        set (PYTHON_SITE_ROOT_DIR
             "${CMAKE_INSTALL_LIBDIR}/python${PYTHON_VERSION_FOUND}/site-packages")
    endif ()
    if (NOT DEFINED PYTHON_SITE_DIR)
        set (PYTHON_SITE_DIR "${PYTHON_SITE_ROOT_DIR}/OpenImageIO")
    endif ()
    message (VERBOSE "    Python site packages dir ${PYTHON_SITE_DIR}")
    message (VERBOSE "    Python site packages root ${PYTHON_SITE_ROOT_DIR}")
    message (VERBOSE "    Python to include 'lib' prefix: ${PYLIB_LIB_PREFIX}")
    message (VERBOSE "    Python to include SO version: ${PYLIB_INCLUDE_SONAME}")
endmacro()


# Help CMake locate nanobind when it was installed as a Python package.
macro (discover_nanobind_cmake_dir)
    if (nanobind_DIR OR nanobind_ROOT OR "$ENV{nanobind_DIR}" OR "$ENV{nanobind_ROOT}")
        return()
    endif ()

    if (NOT Python3_Interpreter_FOUND)
        return()
    endif ()

    execute_process (
        COMMAND ${Python3_EXECUTABLE} -m nanobind --cmake_dir
        RESULT_VARIABLE _oiio_nanobind_result
        OUTPUT_VARIABLE _oiio_nanobind_cmake_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if (_oiio_nanobind_result EQUAL 0
            AND EXISTS "${_oiio_nanobind_cmake_dir}/nanobind-config.cmake")
        set (nanobind_DIR "${_oiio_nanobind_cmake_dir}" CACHE PATH
             "Path to the nanobind CMake package" FORCE)
    endif ()
endmacro()


###########################################################################
# pybind11

macro (setup_python_module)
    cmake_parse_arguments (lib "" "TARGET;MODULE" "SOURCES;LIBS;INCLUDES;SYSTEM_INCLUDE_DIRS" ${ARGN})
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
    target_include_directories (${target_name}
                                PRIVATE ${lib_INCLUDES})
    target_include_directories (${target_name}
                                SYSTEM PRIVATE ${lib_SYSTEM_INCLUDE_DIRS})
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

    if (SKBUILD)
        set (PYTHON_SITE_DIR .)
    endif ()

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

    install (FILES __init__.py stubs/OpenImageIO/__init__.pyi stubs/OpenImageIO/py.typed
             DESTINATION ${PYTHON_SITE_DIR} COMPONENT user)

endmacro ()


###########################################################################
# nanobind

macro (setup_python_module_nanobind)
    cmake_parse_arguments (lib "" "TARGET;MODULE"
                           "SOURCES;LIBS;INCLUDES;SYSTEM_INCLUDE_DIRS;PACKAGE_FILES"
                           ${ARGN})

    set (target_name ${lib_TARGET})

    if (NOT COMMAND nanobind_add_module)
        discover_nanobind_cmake_dir()
        find_package (nanobind CONFIG REQUIRED)
    endif ()

    nanobind_add_module(${target_name} ${lib_SOURCES})
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND TARGET nanobind-static)
        target_compile_options (nanobind-static PRIVATE -Wno-error=format-nonliteral)
    endif ()

    target_include_directories (${target_name}
                                PRIVATE ${lib_INCLUDES})
    target_include_directories (${target_name}
                                SYSTEM PRIVATE ${lib_SYSTEM_INCLUDE_DIRS})
    target_link_libraries (${target_name}
                           PRIVATE ${lib_LIBS})

    set (_module_LINK_FLAGS "${VISIBILITY_MAP_COMMAND} ${EXTRA_DSO_LINK_ARGS}")
    if (UNIX AND NOT APPLE)
        set (_module_LINK_FLAGS "${_module_LINK_FLAGS} -Wl,--exclude-libs,ALL")
    endif ()
    set_target_properties (${target_name} PROPERTIES
                           LINK_FLAGS ${_module_LINK_FLAGS}
                           OUTPUT_NAME ${lib_MODULE}
                           DEBUG_POSTFIX "")

    if (SKBUILD)
        set (_nanobind_install_dir .)
    else ()
        set (_nanobind_install_dir ${PYTHON_SITE_DIR})
    endif ()

    # Keep nanobind modules isolated in the build tree so they don't alter
    # how the existing top-level OpenImageIO module is imported during tests.
    set_target_properties (${target_name} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/python/nanobind/OpenImageIO
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/python/nanobind/OpenImageIO
            )

    install (TARGETS ${target_name}
             RUNTIME DESTINATION ${_nanobind_install_dir} COMPONENT user
             LIBRARY DESTINATION ${_nanobind_install_dir} COMPONENT user)

    if (lib_PACKAGE_FILES)
        install (FILES ${lib_PACKAGE_FILES}
                 DESTINATION ${_nanobind_install_dir} COMPONENT user)
    endif ()
endmacro ()
