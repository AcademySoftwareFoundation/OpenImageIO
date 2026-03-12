# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set (nanobind_experimental_srcs
     nanobind_experimental.cpp)

set (nanobind_build_package_dir
     ${CMAKE_BINARY_DIR}/lib/python/nanobind-experimental/OpenImageIO)
file (MAKE_DIRECTORY ${nanobind_build_package_dir})
configure_file (__init__.py
                ${nanobind_build_package_dir}/__init__.py
                COPYONLY)

setup_python_module_nanobind (
    TARGET    PyOpenImageIONanobindExperimental
    MODULE    _nanobind_experimental
    SOURCES   ${nanobind_experimental_srcs}
)

if (OIIO_PYTHON_BINDINGS_BACKEND STREQUAL "nanobind")
    install (FILES __init__.py
             DESTINATION ${PYTHON_SITE_DIR} COMPONENT user)
endif ()
