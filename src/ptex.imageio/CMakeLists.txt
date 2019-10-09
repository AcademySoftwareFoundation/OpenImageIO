# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (PTEX_FOUND)
    add_oiio_plugin (ptexinput.cpp
                     INCLUDE_DIRS ${ZLIB_INCLUDE_DIRS} ${PTEX_INCLUDE_DIR}
                     LINK_LIBRARIES ${PTEX_LIBRARIES} ${ZLIB_LIBRARIES}
                     DEFINITIONS "-DUSE_PTEX")
endif ()
