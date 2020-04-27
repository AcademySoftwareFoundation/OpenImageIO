# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (Libheif_FOUND)
    add_oiio_plugin (heifinput.cpp heifoutput.cpp
                     LINK_LIBRARIES Libheif::Libheif
                     DEFINITIONS "-DUSE_HEIF=1")
else ()
    message (WARNING "heif plugin will not be built")
endif ()
