// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

// Format reference: Affine Toolkit (Thomas E. Burge), riff.h and riff.c
//                   Autodesk Maya documentation, ilib.h

#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>

#include "imageio_pvt.h"


OIIO_PLUGIN_NAMESPACE_BEGIN

namespace iff_pvt {

// compression numbers
const uint32_t NONE = 0;
const uint32_t RLE  = 1;
const uint32_t QRL  = 2;
const uint32_t QR4  = 3;

const uint32_t RGB     = 0x00000001;
const uint32_t ALPHA   = 0x00000002;
const uint32_t RGBA    = RGB | ALPHA;
const uint32_t ZBUFFER = 0x00000004;
const uint32_t BLACK   = 0x00000010;

// store information about IFF file
class IffFileHeader {
public:
    // header information
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t compression;
    uint8_t pixel_bits;
    uint8_t pixel_channels;
    uint16_t tiles;
    uint16_t tile_width;
    uint16_t tile_height;

    // author string
    std::string author;

    // date string
    std::string date;

    // tbmp start
    uint32_t tbmp_start;

    // for4 start
    uint32_t for4_start;

private:
};



// align size
inline uint32_t
align_size(uint32_t size, uint32_t alignment)
{
    uint32_t mod = size % alignment;
    if (mod) {
        mod = alignment - mod;
        size += mod;
    }
    return size;
}

// tile width
constexpr uint32_t
tile_width()
{
    return 64;
}

// tile height
constexpr uint32_t
tile_height()
{
    return 64;
}

// tile width size
inline uint32_t
tile_width_size(uint32_t width)
{
    uint32_t tw = tile_width();
    return (width + tw - 1) / tw;
}

// tile height size
inline uint32_t
tile_height_size(uint32_t height)
{
    uint32_t th = tile_height();
    return (height + th - 1) / th;
}


}  // namespace iff_pvt


OIIO_PLUGIN_NAMESPACE_END
