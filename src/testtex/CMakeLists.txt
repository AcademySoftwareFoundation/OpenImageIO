# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

set (testtex_srcs testtex.cpp)
add_executable (testtex ${testtex_srcs})
target_include_directories (testtex PRIVATE ${ROBINMAP_INCLUDES})
target_link_libraries (testtex PRIVATE OpenImageIO)
set_target_properties (testtex PROPERTIES FOLDER "Tools")
