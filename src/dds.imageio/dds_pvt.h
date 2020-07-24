// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#pragma once


OIIO_PLUGIN_NAMESPACE_BEGIN


namespace DDS_pvt {

// IneQuation was here

#define DDS_MAKE4CC(a, b, c, d) (a | b << 8 | c << 16 | d << 24)
#define DDS_4CC_DXT1 DDS_MAKE4CC('D', 'X', 'T', '1')
#define DDS_4CC_DXT2 DDS_MAKE4CC('D', 'X', 'T', '2')
#define DDS_4CC_DXT3 DDS_MAKE4CC('D', 'X', 'T', '3')
#define DDS_4CC_DXT4 DDS_MAKE4CC('D', 'X', 'T', '4')
#define DDS_4CC_DXT5 DDS_MAKE4CC('D', 'X', 'T', '5')

/// DDS pixel format flags. Channel flags are only applicable for uncompressed
/// images.
///
enum {
    DDS_PF_ALPHA     = 0x00000001,  ///< image has alpha channel
    DDS_PF_FOURCC    = 0x00000004,  ///< image is compressed
    DDS_PF_LUMINANCE = 0x00020000,  ///< image has luminance data
    DDS_PF_RGB       = 0x00000040   ///< image has RGB data
};

/// DDS pixel format structure.
///
typedef struct {
    uint32_t size;    ///< structure size, must be 32
    uint32_t flags;   ///< flags to indicate valid fields
    uint32_t fourCC;  ///< compression four-character code
    uint32_t bpp;     ///< bits per pixel
    uint32_t rmask;   ///< bitmask for the red channel
    uint32_t gmask;   ///< bitmask for the green channel
    uint32_t bmask;   ///< bitmask for the blue channel
    uint32_t amask;   ///< bitmask for the alpha channel
} dds_pixformat;

/// DDS caps flags, field 1.
///
enum {
    DDS_CAPS1_COMPLEX = 0x00000008,  ///< >2D image or cube map
    DDS_CAPS1_TEXTURE = 0x00001000,  ///< should be set for all DDS files
    DDS_CAPS1_MIPMAP  = 0x00400000   ///< image has mipmaps
};

/// DDS caps flags, field 2.
///
enum {
    DDS_CAPS2_CUBEMAP           = 0x00000200,  ///< image is a cube map
    DDS_CAPS2_CUBEMAP_POSITIVEX = 0x00000400,  ///< +x side
    DDS_CAPS2_CUBEMAP_NEGATIVEX = 0x00000800,  ///< -x side
    DDS_CAPS2_CUBEMAP_POSITIVEY = 0x00001000,  ///< +y side
    DDS_CAPS2_CUBEMAP_NEGATIVEY = 0x00002000,  ///< -y side
    DDS_CAPS2_CUBEMAP_POSITIVEZ = 0x00004000,  ///< +z side
    DDS_CAPS2_CUBEMAP_NEGATIVEZ = 0x00008000,  ///< -z side
    DDS_CAPS2_VOLUME            = 0x00200000   ///< image is a 3D texture
};

/// DDS caps structure.
///
typedef struct {
    uint32_t flags1;  ///< flags to indicate certain surface properties
    uint32_t flags2;  ///< flags to indicate certain surface properties
} dds_caps;

/// DDS global flags - indicate valid header fields.
///
enum {
    DDS_CAPS        = 0x00000001,
    DDS_HEIGHT      = 0x00000002,
    DDS_WIDTH       = 0x00000004,
    DDS_PITCH       = 0x00000008,
    DDS_PIXELFORMAT = 0x00001000,
    DDS_MIPMAPCOUNT = 0x00020000,
    DDS_LINEARSIZE  = 0x00080000,
    DDS_DEPTH       = 0x00800000
};

/// DDS file header.
/// Please note that this layout is not identical to the one found in a file.
///
typedef struct {
    uint32_t fourCC;   ///< file four-character code
    uint32_t size;     ///< structure size, must be 124
    uint32_t flags;    ///< flags to indicate valid fields
    uint32_t height;   ///< image height
    uint32_t width;    ///< image width
    uint32_t pitch;    ///< bytes per scanline (uncmp.)/total byte size (cmp.)
    uint32_t depth;    ///< image depth (for 3D textures)
    uint32_t mipmaps;  ///< number of mipmaps
    // 11 reserved 4-byte fields come in here
    dds_pixformat fmt;  ///< pixel format
    dds_caps caps;      ///< DirectDraw Surface caps
} dds_header;


}  // namespace DDS_pvt


OIIO_PLUGIN_NAMESPACE_END
