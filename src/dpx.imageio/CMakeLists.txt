# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

add_oiio_plugin (dpxinput.cpp dpxoutput.cpp
  libdpx/DPX.cpp libdpx/OutStream.cpp libdpx/RunLengthEncoding.cpp
  libdpx/Codec.cpp libdpx/Reader.cpp libdpx/Writer.cpp libdpx/DPXHeader.cpp
  libdpx/ElementReadStream.cpp libdpx/InStream.cpp libdpx/DPXColorConverter.cpp
  LINK_LIBRARIES ${OPENEXR_LIBRARIES})
