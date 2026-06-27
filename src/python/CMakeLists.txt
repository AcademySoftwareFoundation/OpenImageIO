# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

add_subdirectory (stubs)

set (python_dual_backend_srcs
     py_roi.cpp
     py_typedesc.cpp
     py_imagespec.cpp
     py_paramvalue.cpp)

# Full pybind11 module (pybind11 or both backends).
if (OIIO_BUILD_PYTHON_PYBIND11)
    file (GLOB python_srcs *.cpp)
    setup_python_module (TARGET    PyOpenImageIO
                         MODULE    OpenImageIO
                         SOURCES   ${python_srcs}
                         LIBS      OpenImageIO
                                   ${OPENIMAGEIO_IMATH_TARGETS}
                        )

    # Unity builds: If in unity group mode, make the python bindings one group. If
    # in unity batch mode, use the smaller batch size because these tend to be
    # expensive files to compile.
    set_target_properties(PyOpenImageIO PROPERTIES
                               UNITY_BUILD_BATCH_SIZE ${UNITY_SMALL_BATCH_SIZE})
    set_source_files_properties(${python_srcs} PROPERTIES
                                 UNITY_GROUP PyOpenImageIO)
endif ()

# Nanobind-only backend: compile the py_*.cpp files in this directory
# with nanobind and install as the main OpenImageIO Python package.
# When backend is both, skip this; nanobind is built under src/python-nanobind/.
if (OIIO_PYTHON_BINDINGS_BACKEND STREQUAL "nanobind")
    setup_python_module_nanobind (
        TARGET    PyOpenImageIO
        MODULE    OpenImageIO
        SOURCES   py_oiio.cpp ${python_dual_backend_srcs}
        LIBS      OpenImageIO
    )

    target_compile_definitions (PyOpenImageIO
                                PRIVATE OIIO_PY_BACKEND_NANOBIND)
endif ()
