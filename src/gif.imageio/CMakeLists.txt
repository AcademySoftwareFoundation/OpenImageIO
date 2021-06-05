# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (GIF_FOUND)
    add_oiio_plugin (gifinput.cpp gifoutput.cpp
                     LINK_LIBRARIES GIF::GIF
                     DEFINITIONS "-DUSE_GIF")
else()
    message (WARNING "GIF plugin will not be built")
endif()
