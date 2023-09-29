# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

add_oiio_plugin (jpeginput.cpp jpegoutput.cpp
                 LINK_LIBRARIES
                     $<TARGET_NAME_IF_EXISTS:libjpeg-turbo::jpeg>
                     $<TARGET_NAME_IF_EXISTS:JPEG::JPEG>
                 )
