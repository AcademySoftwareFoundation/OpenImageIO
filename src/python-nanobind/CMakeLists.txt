# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set (nanobind_experimental_srcs
     nanobind_experimental.cpp)

setup_python_module_nanobind (
    TARGET    PyOpenImageIONanobindExperimental
    MODULE    _nanobind_experimental
    SOURCES   ${nanobind_experimental_srcs}
)

if (OIIO_PYTHON_BINDINGS_BACKEND STREQUAL "nanobind")
    install (FILES __init__.py
             DESTINATION ${PYTHON_SITE_DIR} COMPONENT user)
endif ()
