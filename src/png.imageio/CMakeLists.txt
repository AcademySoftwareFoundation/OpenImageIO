# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

if (TARGET PNG::PNG)
    add_oiio_plugin (pnginput.cpp pngoutput.cpp
                     LINK_LIBRARIES PNG::PNG ZLIB::ZLIB)
else ()
    message (WARNING "libpng not found, so PNG support will not work")
    set (format_plugin_definitions ${format_plugin_definitions} DISABLE_PNG=1 PARENT_SCOPE)
endif ()

