# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

add_oiio_plugin (cineoninput.cpp
                 libcineon/Cineon.cpp libcineon/OutStream.cpp libcineon/Codec.cpp
                 libcineon/Reader.cpp libcineon/Writer.cpp libcineon/CineonHeader.cpp
                 libcineon/ElementReadStream.cpp libcineon/InStream.cpp)
