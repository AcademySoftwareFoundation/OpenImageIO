# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO



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
