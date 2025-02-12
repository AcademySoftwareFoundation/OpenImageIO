// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once


// Some documentation for the DDS format:
// https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide
// https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-reference

OIIO_PLUGIN_NAMESPACE_BEGIN


namespace DDS_pvt {

#define DDS_MAKE4CC(a, b, c, d) (a | b << 8 | c << 16 | d << 24)
#define DDS_4CC_DXT1 DDS_MAKE4CC('D', 'X', 'T', '1')
#define DDS_4CC_DXT2 DDS_MAKE4CC('D', 'X', 'T', '2')
#define DDS_4CC_DXT3 DDS_MAKE4CC('D', 'X', 'T', '3')
#define DDS_4CC_DXT4 DDS_MAKE4CC('D', 'X', 'T', '4')
#define DDS_4CC_DXT5 DDS_MAKE4CC('D', 'X', 'T', '5')
#define DDS_4CC_ATI1 DDS_MAKE4CC('A', 'T', 'I', '1')
#define DDS_4CC_ATI2 DDS_MAKE4CC('A', 'T', 'I', '2')
#define DDS_4CC_DX10 DDS_MAKE4CC('D', 'X', '1', '0')
#define DDS_4CC_RXGB DDS_MAKE4CC('R', 'X', 'G', 'B')
#define DDS_4CC_BC4U DDS_MAKE4CC('B', 'C', '4', 'U')
#define DDS_4CC_BC5U DDS_MAKE4CC('B', 'C', '5', 'U')

#define DDS_FORMAT_R10G10B10A2_UNORM 24
#define DDS_FORMAT_R8G8B8A8_UNORM 28
#define DDS_FORMAT_R8G8B8A8_UNORM_SRGB 29
#define DDS_FORMAT_R16_UNORM 56
#define DDS_FORMAT_BC1_UNORM 71
#define DDS_FORMAT_BC1_UNORM_SRGB 72
#define DDS_FORMAT_BC2_UNORM 74
#define DDS_FORMAT_BC2_UNORM_SRGB 75
#define DDS_FORMAT_BC3_UNORM 77
#define DDS_FORMAT_BC3_UNORM_SRGB 78
#define DDS_FORMAT_BC4_UNORM 80
#define DDS_FORMAT_BC5_UNORM 83
#define DDS_FORMAT_B8G8R8A8_UNORM 87
#define DDS_FORMAT_B8G8R8X8_UNORM 88
#define DDS_FORMAT_B8G8R8A8_UNORM_SRGB 91
#define DDS_FORMAT_B8G8R8X8_UNORM_SRGB 93
#define DDS_FORMAT_BC6H_UF16 95
#define DDS_FORMAT_BC6H_SF16 96
#define DDS_FORMAT_BC7_UNORM 98
#define DDS_FORMAT_BC7_UNORM_SRGB 99

enum class Compression {
    None,
    DXT1,  // aka BC1
    DXT2,
    DXT3,  // aka BC2
    DXT4,
    DXT5,  // aka BC3
    BC4,   // aka ATI1
    BC5,   // aka ATI2
    BC6HU,
    BC6HS,
    BC7
};

/// DDS pixel format flags. Channel flags are only applicable for uncompressed
/// images.
///
enum {
    DDS_PF_ALPHA     = 0x00000001,   ///< image has alpha channel
    DDS_PF_ALPHAONLY = 0x00000002,   ///< image has only the alpha channel
    DDS_PF_FOURCC    = 0x00000004,   ///< image is compressed
    DDS_PF_LUMINANCE = 0x00020000,   ///< image has luminance data
    DDS_PF_RGB       = 0x00000040,   ///< image has RGB data
    DDS_PF_YUV       = 0x00000200,   ///< image has YUV data
    DDS_PF_NORMAL    = 0x80000000u,  ///< image is a tangent space normal map
};

/// DDS pixel format structure.
///
typedef struct {
    uint32_t size;      ///< structure size, must be 32
    uint32_t flags;     ///< flags to indicate valid fields
    uint32_t fourCC;    ///< compression four-character code
    uint32_t bpp;       ///< bits per pixel
    uint32_t masks[4];  ///< bitmasks for the r,g,b,a channels
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
    uint32_t flags1;
    uint32_t flags2;
    uint32_t flags3;
    uint32_t flags4;
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
typedef struct {
    uint32_t fourCC;   ///< file four-character code
    uint32_t size;     ///< structure size, must be 124
    uint32_t flags;    ///< flags to indicate valid fields
    uint32_t height;   ///< image height
    uint32_t width;    ///< image width
    uint32_t pitch;    ///< bytes per scanline (uncmp.)/total byte size (cmp.)
    uint32_t depth;    ///< image depth (for 3D textures)
    uint32_t mipmaps;  ///< number of mipmaps
    uint32_t unused0[11];
    dds_pixformat fmt;  ///< pixel format
    dds_caps caps;      ///< DirectDraw Surface caps
    uint32_t unused1;
} dds_header;

/// Optional header for images in DX10+ formats.
typedef struct {
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlag2;
} dds_header_dx10;


}  // namespace DDS_pvt


OIIO_PLUGIN_NAMESPACE_END
