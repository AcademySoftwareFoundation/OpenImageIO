# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

# from pythonutils.cmake
checked_find_package (pybind11 REQUIRED
                      VERSION_MIN 2.4.2)


file (GLOB python_srcs *.cpp)
setup_python_module (TARGET    PyOpenImageIO
                     MODULE    OpenImageIO
                     SOURCES   ${python_srcs}
                     LIBS      OpenImageIO
                     )
