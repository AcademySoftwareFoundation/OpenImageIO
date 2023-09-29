// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace softimage_pvt {

class PicFileHeader {
public:
    // Read pic header from file
    bool read_header(FILE* fd);

    // PIC header
    uint32_t magic;    // Softimage magic number
    float version;     // Storage format - 1 is RLE, 0 is RAW
    char comment[80];  // Comment
    char id[4];        // ID - should be PICT
    uint16_t width;    // X size in pixels
    uint16_t height;   // Y size in pixels
    float ratio;       // Pixel aspect ratio
    uint16_t fields;   // The scanline setting - No Pictures, Odd, Even or every
    uint16_t pad;      // unused

private:
    void swap_endian();
};  // class PicFileHeader



class ChannelPacket {
public:
    //channel packet contains info on the image data
    ChannelPacket() = default;
    // !brief  get a list of the channels contained in this channel packet
    std::vector<int> channels() const;
    uint8_t chained     = 0;  // 0 if this is the last channel packet
    uint8_t size        = 0;  // Number of bits per pixel per channel
    uint8_t type        = 0;  // Data encoding and type
    uint8_t channelCode = 0;  // bitset for channels
};



enum channelCodes {
    RED_CHANNEL   = 0x80,
    GREEN_CHANNEL = 0x40,
    BLUE_CHANNEL  = 0x20,
    ALPHA_CHANNEL = 0x10
};  // enum channelCodes



enum encoding {
    UNCOMPRESSED,
    PURE_RUN_LENGTH,
    MIXED_RUN_LENGTH
};  // enum encoding

}  //namespace softimage_pvt

OIIO_PLUGIN_NAMESPACE_END
