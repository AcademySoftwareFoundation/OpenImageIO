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
                          COMPONENTS ${_py_components}
                          PRINT Python3_VERSION Python3_EXECUTABLE
                                Python3_LIBRARIES
                                Python3_Development_FOUND
                                Python3_Development.Module_FOUND
                                Python3_Interpreter_FOUND )

    # The version that was found may not be the default or user
    # defined one.
    set (PYTHON_VERSION_FOUND ${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR})

    # Give hints to subsequent pybind11 searching to ensure that it finds
    # exactly the same version that we found.
    set (PythonInterp3_FIND_VERSION PYTHON_VERSION_FOUND)
    set (PythonInterp3_FIND_VERSION_MAJOR ${Python3_VERSION_MAJOR})

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

    set(PYTHON_BUILD_SITE "${CMAKE_BINARY_DIR}/lib/python/site-packages")

    # In the build area, put it in lib/python so it doesn't clash with the
    # non-python libraries of the same name (which aren't prefixed by "lib"
    # on Windows).
    set_target_properties (${target_name} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY ${PYTHON_BUILD_SITE}
            ARCHIVE_OUTPUT_DIRECTORY ${PYTHON_BUILD_SITE}
            )

    install (TARGETS ${target_name}
             RUNTIME DESTINATION ${PYTHON_SITE_DIR} COMPONENT user
             LIBRARY DESTINATION ${PYTHON_SITE_DIR} COMPONENT user)

    install(FILES __init__.py DESTINATION ${PYTHON_SITE_DIR} COMPONENT user)

    # Create the __init__.pyi stub file
    if (PYTHON_VERSION_FOUND VERSION_GREATER_EQUAL "3.9")
        # A modern version of python is required for the necessary version of mypy

        # Run stub generation process
        set (_stub_file "${CMAKE_BINARY_DIR}/lib/python/site-packages/OpenImageIO.pyi")
        # FIXME: is this the right location to use?  the source gets copied to build/src
        set (_stub_gen "${CMAKE_SOURCE_DIR}/src/python/generate_stubs.py")

        find_program (PIPX_EXE NAMES pipx pipx.exe)

        if (NOT PIPX_EXE)
            # add_custom_command (
            #     COMMAND ${Python3_EXECUTABLE} -m venv "${PYTHON_VENV_DIR}"
            #     COMMAND ${PYTHON_VENV_EXE} -m pip install uv
            #     OUTPUT "${PYTHON_VENV_DIR}/bin/activate"
            #     COMMENT "Creating virtualenv at ${PYTHON_VENV_DIR}"
            # )
            message(INFO "Installing pipx")
            execute_process (
                COMMAND ${Python3_EXECUTABLE} -m venv "${PYTHON_VENV_DIR}"
                COMMAND ${PYTHON_VENV_EXE} -m pip install pipx
                COMMAND_ERROR_IS_FATAL ANY
            )
            find_program (PIPX_EXE NAMES pipx pipx.exe)
            if (NOT PIPX_EXE)
                message(FATAL_ERROR "Could not find or install pipx")
            endif()
        endif()

        add_custom_command (
            COMMAND ${PIPX_EXE} run ${_stub_gen} ${PYTHON_BUILD_SITE}
            OUTPUT ${_stub_file}
            DEPENDS ${_stub_gen}
            COMMENT "Creating python stubs")
        install (FILES ${_stub_file} DESTINATION ${PYTHON_SITE_DIR} RENAME __init__.pyi COMPONENT user)
        # install the marker file
        file (WRITE "${CMAKE_BINARY_DIR}/lib/python/site-packages/py.typed" "")
        install (FILES "${CMAKE_BINARY_DIR}/lib/python/site-packages/py.typed"
                DESTINATION ${PYTHON_SITE_DIR} COMPONENT user)

        # Ensure this runs after PyOpenImageIO
        add_custom_target (
            PyOpenImageIO_stubs ALL
            DEPENDS ${_stub_file} "${CMAKE_BINARY_DIR}/lib/python/site-packages/py.typed")
        add_dependencies (PyOpenImageIO_stubs PyOpenImageIO)
    endif()

endmacro ()

