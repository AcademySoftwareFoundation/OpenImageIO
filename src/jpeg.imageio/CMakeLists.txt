# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/OpenImageIO/oiio

add_oiio_plugin (jpeginput.cpp jpegoutput.cpp
                 INCLUDE_DIRS ${JPEG_INCLUDE_DIRS}
                 LINK_LIBRARIES JPEG::JPEG)
