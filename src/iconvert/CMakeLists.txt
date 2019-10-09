# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

set (iconvert_srcs iconvert.cpp)
add_executable (iconvert ${iconvert_srcs})
target_link_libraries (iconvert PRIVATE OpenImageIO)
set_target_properties (iconvert PROPERTIES FOLDER "Tools")
oiio_install_targets (iconvert)
