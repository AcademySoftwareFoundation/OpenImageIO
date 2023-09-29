// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

// Format reference: ftp://ftp.sgi.com/graphics/SGIIMAGESPEC

#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace sgi_pvt {

// magic number identifying SGI file
const short SGI_MAGIC = 0x01DA;

// SGI file header - all fields are written in big-endian to file
struct SgiHeader {
    int16_t magic;       // must be 0xDA01 (big-endian)
    int8_t storage;      // compression used, see StorageFormat enum
    int8_t bpc;          // number of bytes per pixel channel
    uint16_t dimension;  // dimension of he image, see Dimension
    uint16_t xsize;      // width in pixels
    uint16_t ysize;      // height in pixels
    uint16_t zsize;      // number of channels: 1(B/W), 3(RGB) or 4(RGBA)
    int32_t pixmin;      // minimum pixel value
    int32_t pixmax;      // maximum pixel value
    int32_t dummy;       // unused, should be set to 0
    char imagename[80];  // null terminated ASCII string
    int32_t colormap;    // how pixels should be interpreted
                         // see ColorMap enum
};

// size of the header with all dummy bytes
const int SGI_HEADER_LEN = 512;

enum StorageFormat {
    VERBATIM = 0,  // uncompressed
    RLE            // RLE compressed
};

enum Dimension {
    ONE_SCANLINE_ONE_CHANNEL = 1,  // single scanline and single channel
    MULTI_SCANLINE_ONE_CHANNEL,    // multiscanline, single channel
    MULTI_SCANLINE_MULTI_CHANNEL   // multiscanline, multichannel
};

enum ColorMap {
    NORMAL = 0,  // B/W image for 1 channel, RGB for 3 channels and RGBA for 4
    DITHERED,  // only one channel of data, RGB values are packed int one byte:
               // red and green - 3 bits, blue - 2 bits; obsolete
    SCREEN,    // obsolete
    COLORMAP   // TODO: what is this?
};

}  // namespace sgi_pvt


OIIO_PLUGIN_NAMESPACE_END
