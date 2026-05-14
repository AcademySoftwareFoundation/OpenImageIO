# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set (nanobind_srcs
     py_oiio.cpp
     py_paramvalue.cpp
     py_roi.cpp
     py_imagespec.cpp
     py_typedesc.cpp)

set (nanobind_build_package_dir ${CMAKE_BINARY_DIR}/lib/python/nanobind/OpenImageIO)
file (MAKE_DIRECTORY ${nanobind_build_package_dir})
configure_file (__init__.py
                ${nanobind_build_package_dir}/__init__.py
                COPYONLY)

setup_python_module_nanobind (
    TARGET    PyOpenImageIONanobind
    MODULE    _OpenImageIO
    SOURCES   ${nanobind_srcs}
    LIBS      OpenImageIO
)

if (OIIO_PYTHON_BINDINGS_BACKEND STREQUAL "nanobind")
    if (SKBUILD)
        install (FILES __init__.py DESTINATION . COMPONENT user)
    else ()
        install (FILES __init__.py
                 DESTINATION ${PYTHON_SITE_DIR} COMPONENT user)
    endif ()
endif ()
