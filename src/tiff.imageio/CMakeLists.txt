# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

add_oiio_plugin (tiffinput.cpp tiffoutput.cpp
                 LINK_LIBRARIES TIFF::TIFF
                                ZLIB::ZLIB
                                $<TARGET_NAME_IF_EXISTS:libjpeg-turbo::jpeg>
                                $<TARGET_NAME_IF_EXISTS:JPEG::JPEG>
                )

# TODO: It's not clear that we need to explicitly list the ZLIB and JPEG
# targets here, or if TIFF::TIFF is expected to carry them along if needed.
