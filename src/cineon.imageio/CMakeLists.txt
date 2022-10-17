# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

add_oiio_plugin (cineoninput.cpp
                 libcineon/Cineon.cpp libcineon/OutStream.cpp libcineon/Codec.cpp
                 libcineon/Reader.cpp libcineon/CineonHeader.cpp
                 libcineon/ElementReadStream.cpp libcineon/InStream.cpp)

# Note: OIIO doesn't support cineon output, so we don't compile
# libcineon/Writer.cpp
