# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

set (idiff_srcs idiff.cpp)
add_executable (idiff ${idiff_srcs})
target_link_libraries (idiff PRIVATE OpenImageIO)
set_target_properties (idiff PROPERTIES FOLDER "Tools")
oiio_install_targets (idiff)
