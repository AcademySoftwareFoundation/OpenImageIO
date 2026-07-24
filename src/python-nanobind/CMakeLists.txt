# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set (nanobind_srcs
     ${CMAKE_CURRENT_SOURCE_DIR}/../python/py_oiio.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/../python/py_roi.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/../python/py_typedesc.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/../python/py_imagespec.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/../python/py_paramvalue.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/../python/py_deepdata.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/../python/py_colorconfig.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/../python/py_imageinput.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/../python/py_imageoutput.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/../python/py_imagebuf.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/../python/py_imagecache.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/../python/py_texturesys.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/../python/py_imagebufalgo.cpp)

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
    PACKAGE_FILES __init__.py
)

target_compile_definitions (PyOpenImageIONanobind
                            PRIVATE OIIO_PY_BACKEND_NANOBIND
                                    OIIO_PY_NANOBIND_ISOLATED_PACKAGE)
target_include_directories (PyOpenImageIONanobind
                            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../python)
