# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

add_oiio_plugin (tiffinput.cpp tiffoutput.cpp
                 INCLUDE_DIRS ${TIFF_INCLUDE_DIR}
                 LINK_LIBRARIES ${TIFF_LIBRARIES} ${JPEG_LIBRARIES}
                                ZLIB::ZLIB)
