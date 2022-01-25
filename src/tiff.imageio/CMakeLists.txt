# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

add_oiio_plugin (tiffinput.cpp tiffoutput.cpp
                 LINK_LIBRARIES TIFF::TIFF JPEG::JPEG ZLIB::ZLIB)
