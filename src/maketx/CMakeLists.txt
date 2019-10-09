# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

set (maketx_srcs maketx.cpp)
add_executable (maketx ${maketx_srcs})
target_link_libraries (maketx PRIVATE OpenImageIO)
set_target_properties (maketx PROPERTIES FOLDER "Tools")
oiio_install_targets (maketx)
