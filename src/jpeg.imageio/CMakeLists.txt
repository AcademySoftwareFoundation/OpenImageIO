# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

add_oiio_plugin (jpeginput.cpp jpegoutput.cpp
                 INCLUDE_DIRS ${JPEG_INCLUDE_DIR}
                 LINK_LIBRARIES ${JPEG_LIBRARIES})
