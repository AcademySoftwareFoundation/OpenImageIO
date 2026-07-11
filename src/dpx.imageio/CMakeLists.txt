# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

add_oiio_plugin (dpxinput.cpp dpxoutput.cpp
  libdpx/DPX.cpp libdpx/OutStream.cpp libdpx/RunLengthEncoding.cpp
  libdpx/Codec.cpp libdpx/Reader.cpp libdpx/Writer.cpp libdpx/DPXHeader.cpp
  libdpx/ElementReadStream.cpp libdpx/InStream.cpp libdpx/DPXColorConverter.cpp
  LINK_LIBRARIES
    # dpxinput.cpp uses Imf::TimeCode from OpenEXR. In the embedded build this
    # comes in transitively via libOpenImageIO; a standalone plugin must link
    # it explicitly.
    $<TARGET_NAME_IF_EXISTS:OpenEXR::OpenEXR>
  )
