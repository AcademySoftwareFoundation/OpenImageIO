# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (GIF_FOUND)
    add_oiio_plugin (gifinput.cpp gifoutput.cpp
                     LINK_LIBRARIES GIF::GIF
                     DEFINITIONS "USE_GIF")
else()
    message (WARNING "GIF plugin will not be built")
endif()
