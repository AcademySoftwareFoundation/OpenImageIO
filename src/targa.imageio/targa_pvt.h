// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

#include <OpenImageIO/oiioversion.h>


OIIO_PLUGIN_NAMESPACE_BEGIN

namespace TGA_pvt {


enum tga_image_type {
    TYPE_NODATA       = 0,   ///< image with no data (why even spec it?)
    TYPE_PALETTED     = 1,   ///< paletted RGB
    TYPE_RGB          = 2,   ///< can include alpha
    TYPE_GRAY         = 3,   ///< can include alpha
    TYPE_PALETTED_RLE = 9,   ///< same as PALETTED but run-length encoded
    TYPE_RGB_RLE      = 10,  ///< same as RGB but run-length encoded
    TYPE_GRAY_RLE     = 11   ///< same as GRAY but run-length encoded
};


enum tga_flags {
    FLAG_X_FLIP = 0x10,  ///< right-left image
    FLAG_Y_FLIP = 0x20   ///< top-down image
};


/// Targa file header.
typedef struct {
    uint8_t idlen;         ///< image comment length
    uint8_t cmap_type;     ///< palette type
    uint8_t type;          ///< image type (see tga_image_type)
    uint16_t cmap_first;   ///< offset to first entry
    uint16_t cmap_length;  ///<
    uint8_t cmap_size;     ///< palette size
    uint16_t x_origin;     ///<
    uint16_t y_origin;     ///<
    uint16_t width;        ///< image width
    uint16_t height;       ///< image height
    uint8_t bpp;           ///< bits per pixel
    uint8_t attr;          ///< attribs (alpha bits and \ref tga_flags)
} tga_header;


/// TGA 2.0 file footer.
typedef struct {
    uint32_t ofs_ext;    ///< offset to the extension area
    uint32_t ofs_dev;    ///< offset to the developer directory
    char signature[18];  ///< file signature string
} tga_footer;


/// TGA 2.0 developer directory entry
typedef struct {
    uint16_t tag;   ///< tag
    uint32_t ofs;   ///< byte offset to the tag data
    uint32_t size;  ///< tag data length
} tga_devdir_tag;


// this is used in the extension area
enum tga_alpha_type {
    TGA_ALPHA_NONE             = 0,  ///< no alpha data included
    TGA_ALPHA_UNDEFINED_IGNORE = 1,  ///< can ignore alpha
    TGA_ALPHA_UNDEFINED_RETAIN = 2,  ///< undefined, but should be retained
    TGA_ALPHA_USEFUL           = 3,  ///< useful alpha data is present
    TGA_ALPHA_PREMULTIPLIED    = 4,  ///< alpha is pre-multiplied (arrrgh!)
    TGA_ALPHA_INVALID                // one past the last valid value
    // values 5-127 are reserved
    // values 128-255 are unassigned
};

}  // namespace TGA_pvt



OIIO_PLUGIN_NAMESPACE_END
