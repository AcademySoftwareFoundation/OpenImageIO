# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (LIBHEIF_FOUND)
    add_oiio_plugin (heifinput.cpp heifoutput.cpp
                     INCLUDE_DIRS ${LIBHEIF_INCLUDES}
                     LINK_LIBRARIES ${LIBHEIF_LIBRARIES}
                     DEFINITIONS "-DUSE_HEIF=1")
else ()
    message (WARNING "heif plugin will not be built")
endif ()
