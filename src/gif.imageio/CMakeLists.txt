# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (GIF_FOUND)
    add_oiio_plugin (gifinput.cpp gifoutput.cpp
                     INCLUDE_DIRS ${GIF_INCLUDE_DIRS}
                     LINK_LIBRARIES ${GIF_LIBRARIES}
                     DEFINITIONS "-DUSE_GIF")
else()
    message (WARNING "GIF plugin will not be built")
endif()
