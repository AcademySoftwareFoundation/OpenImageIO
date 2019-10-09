# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

add_oiio_plugin (zfile.cpp
                 INCLUDE_DIRS ${ZLIB_INCLUDE_DIRS}
                 LINK_LIBRARIES ${ZLIB_LIBRARIES})
