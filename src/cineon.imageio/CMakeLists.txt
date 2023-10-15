# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

add_oiio_plugin (cineoninput.cpp
                 libcineon/Cineon.cpp libcineon/OutStream.cpp libcineon/Codec.cpp
                 libcineon/Reader.cpp libcineon/CineonHeader.cpp
                 libcineon/ElementReadStream.cpp libcineon/InStream.cpp)

# Note: OIIO doesn't support cineon output, so we don't compile
# libcineon/Writer.cpp
