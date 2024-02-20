// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "softimage_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

// Obligatory material to make this a recognizable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int softimage_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
softimage_imageio_library_version()
{
    return nullptr;
}

OIIO_PLUGIN_EXPORTS_END



namespace softimage_pvt {


void
PicFileHeader::swap_endian()
{
    OIIO::swap_endian(&magic);
    OIIO::swap_endian(&width);
    OIIO::swap_endian(&height);
    OIIO::swap_endian(&version);
    OIIO::swap_endian(&ratio);
    OIIO::swap_endian(&fields);
}



bool
PicFileHeader::read_header(FILE* fd)
{
    int byte_count = 0;
    byte_count += fread(this, 1, sizeof(PicFileHeader), fd);

    // Check if we're running on a little endian processor
    if (littleendian())
        swap_endian();

    return (byte_count == sizeof(PicFileHeader));
}



std::vector<int>
ChannelPacket::channels() const
{
    std::vector<int> chanMap;

    // Check for the channels and add them to the chanMap
    if (channelCode & RED_CHANNEL)
        chanMap.push_back(0);
    if (channelCode & GREEN_CHANNEL)
        chanMap.push_back(1);
    if (channelCode & BLUE_CHANNEL)
        chanMap.push_back(2);
    if (channelCode & ALPHA_CHANNEL)
        chanMap.push_back(3);

    return chanMap;
}


}  // namespace softimage_pvt

OIIO_PLUGIN_NAMESPACE_END
