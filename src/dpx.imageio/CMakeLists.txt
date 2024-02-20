# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

add_oiio_plugin (dpxinput.cpp dpxoutput.cpp
  libdpx/DPX.cpp libdpx/OutStream.cpp libdpx/RunLengthEncoding.cpp
  libdpx/Codec.cpp libdpx/Reader.cpp libdpx/Writer.cpp libdpx/DPXHeader.cpp
  libdpx/ElementReadStream.cpp libdpx/InStream.cpp libdpx/DPXColorConverter.cpp
  )
