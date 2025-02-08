// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdint.h>

#include <OpenImageIO/dassert.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/parallel.h>
#include <OpenImageIO/typedesc.h>

#include "dds_pvt.h"
#define BCDEC_IMPLEMENTATION
#include "bcdec.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace DDS_pvt;

constexpr int kBlockSize = 4;

// uncomment the following define to enable 3x2 cube map layout
//#define DDS_3X2_CUBE_MAP_LAYOUT

class DDSInput final : public ImageInput {
public:
    DDSInput() { init(); }
    ~DDSInput() override { close(); }
    const char* format_name(void) const override { return "dds"; }
    int supports(string_view feature) const override
    {
        return feature == "ioproxy";
    }
    bool valid_file(Filesystem::IOProxy* ioproxy) const override;
    bool open(const std::string& name, ImageSpec& newspec) override;
    bool open(const std::string& name, ImageSpec& spec,
              const ImageSpec& config) override;
    bool close() override;
    int current_subimage(void) const override
    {
        lock_guard lock(*this);
        return m_subimage;
    }
    int current_miplevel(void) const override
    {
        lock_guard lock(*this);
        return m_miplevel;
    }
    bool seek_subimage(int subimage, int miplevel) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool read_native_tile(int subimage, int miplevel, int x, int y, int z,
                          void* data) override;

private:
    std::string m_filename;            ///< Stash the filename
    std::vector<unsigned char> m_buf;  ///< Buffer the image pixels
    int m_subimage;
    int m_miplevel;
    int m_nchans;               ///< Number of colour channels in image
    int m_nfaces;               ///< Number of cube map sides in image
    int m_Bpp;                  ///< Number of bytes per pixel
    uint32_t m_BitCounts[4];    ///< Bit counts in r,g,b,a channels
    uint32_t m_RightShifts[4];  ///< Shifts to extract r,g,b,a channels
    Compression m_compression = Compression::None;
    dds_header m_dds;  ///< DDS header
    dds_header_dx10 m_dx10;

    /// Reset everything to initial state
    ///
    void init()
    {
        m_subimage = -1;
        m_miplevel = -1;
        m_buf.clear();
        ioproxy_clear();
    }

    /// Helper function: read the image as scanlines (all but cubemaps).
    ///
    bool readimg_scanlines();

    /// Helper function: read the image as tiles (cubemaps only).
    ///
    bool readimg_tiles();

    /// Helper function: calculate bit shifts to properly extract channel data
    ///
    inline static void calc_shifts(uint32_t mask, uint32_t& count,
                                   uint32_t& right);

    /// Helper function: performs the actual file seeking.
    ///
    void internal_seek_subimage(int cubeface, int miplevel, unsigned int& w,
                                unsigned int& h, unsigned int& d);

    /// Helper function: performs the actual pixel decoding.
    bool internal_readimg(unsigned char* dst, int w, int h, int d);

    static bool validate_signature(uint32_t signature);
};

static TypeDesc::BASETYPE
GetBaseType(Compression cmp)
{
    if (cmp == Compression::BC6HU || cmp == Compression::BC6HS)
        return TypeDesc::HALF;
    return TypeDesc::UINT8;
}

static int
GetChannelCount(Compression cmp, bool isNormal)
{
    if (cmp == Compression::DXT5)
        return isNormal ? 3 : 4;
    if (cmp == Compression::BC5)
        return isNormal ? 3 : 2;
    if (cmp == Compression::BC4)
        return 1;
    if (cmp == Compression::BC6HU || cmp == Compression::BC6HS)
        return 3;
    return 4;
}

static size_t
GetBlockSize(Compression cmp)
{
    return cmp == Compression::DXT1 || cmp == Compression::BC4 ? 8 : 16;
}

static size_t
GetStorageRequirements(size_t width, size_t height, Compression cmp)
{
    size_t blockCount = ((width + kBlockSize - 1) / kBlockSize)
                        * ((height + kBlockSize - 1) / kBlockSize);
    return blockCount * GetBlockSize(cmp);
}

static uint8_t
ComputeNormalZ(uint8_t x, uint8_t y)
{
    float nx  = 2 * (x / 255.0f) - 1;
    float ny  = 2 * (y / 255.0f) - 1;
    float nz  = 0.0f;
    float nz2 = 1 - nx * nx - ny * ny;
    if (nz2 > 0) {
        nz = sqrtf(nz2);
    }
    int z = int(255.0f * (nz + 1) / 2.0f);
    if (z < 0)
        z = 0;
    if (z > 255)
        z = 255;
    return z;
}

static void
ComputeNormalRG(uint8_t rgba[kBlockSize * kBlockSize * 4])
{
    // expand from RG into RGB, computing B from RG
    for (int i = kBlockSize * kBlockSize - 1; i >= 0; --i) {
        uint8_t x       = rgba[i * 2 + 0];
        uint8_t y       = rgba[i * 2 + 1];
        rgba[i * 3 + 0] = x;
        rgba[i * 3 + 1] = y;
        rgba[i * 3 + 2] = ComputeNormalZ(x, y);
    }
}

static void
ComputeNormalAG(uint8_t rgba[kBlockSize * kBlockSize * 4])
{
    // contract from RGBA (R & B unused) to RGB, computing B from GA
    for (int i = 0; i < kBlockSize * kBlockSize; ++i) {
        uint8_t x       = rgba[i * 4 + 3];
        uint8_t y       = rgba[i * 4 + 1];
        rgba[i * 3 + 0] = x;
        rgba[i * 3 + 1] = y;
        rgba[i * 3 + 2] = ComputeNormalZ(x, y);
    }
}


static void
DecompressImage(uint8_t* rgba, int width, int height, const uint8_t* blocks,
                Compression cmp, const dds_pixformat& pixelFormat, int nthreads)
{
    const size_t blockSize = GetBlockSize(cmp);
    const int channelCount = GetChannelCount(cmp,
                                             pixelFormat.flags & DDS_PF_NORMAL);

    const int widthInBlocks  = (width + kBlockSize - 1) / kBlockSize;
    const int heightInBlocks = (height + kBlockSize - 1) / kBlockSize;
    paropt opt               = paropt(nthreads, paropt::SplitDir::Y, 8);
    parallel_for_chunked(
        0, heightInBlocks, 0,
        [&](int64_t ybb, int64_t ybe) {
            uint8_t rgbai[kBlockSize * kBlockSize * 4];
            uint16_t rgbh[kBlockSize * kBlockSize * 3];
            const int ybegin         = int(ybb) * kBlockSize;
            const int yend           = std::min(int(ybe) * kBlockSize, height);
            const uint8_t* srcBlocks = blocks + ybb * widthInBlocks * blockSize;
            for (int y = ybegin; y < yend; y += kBlockSize) {
                for (int x = 0; x < width; x += kBlockSize) {
                    // decompress the BCn block
                    switch (cmp) {
                    case Compression::DXT1:
                        bcdec_bc1(srcBlocks, rgbai, kBlockSize * 4);
                        break;
                    case Compression::DXT2:
                    case Compression::DXT3:
                        bcdec_bc2(srcBlocks, rgbai, kBlockSize * 4);
                        break;
                    case Compression::DXT4:
                    case Compression::DXT5:
                        bcdec_bc3(srcBlocks, rgbai, kBlockSize * 4);
                        break;
                    case Compression::BC4:
                        bcdec_bc4(srcBlocks, rgbai, kBlockSize);
                        break;
                    case Compression::BC5:
                        bcdec_bc5(srcBlocks, rgbai, kBlockSize * 2);
                        break;
                    case Compression::BC6HU:
                    case Compression::BC6HS:
                        bcdec_bc6h_half(srcBlocks, rgbh, kBlockSize * 3,
                                        cmp == Compression::BC6HS);
                        break;
                    case Compression::BC7:
                        bcdec_bc7(srcBlocks, rgbai, kBlockSize * 4);
                        break;
                    default: return;
                    }
                    srcBlocks += blockSize;

                    // Swap R & A for RXGB format case
                    if (cmp == Compression::DXT5
                        && pixelFormat.fourCC == DDS_4CC_RXGB) {
                        for (int i = 0; i < 16; ++i) {
                            uint8_t r        = rgbai[i * 4 + 0];
                            uint8_t a        = rgbai[i * 4 + 3];
                            rgbai[i * 4 + 0] = a;
                            rgbai[i * 4 + 3] = r;
                        }
                    }
                    // Convert into full normal map if needed
                    else if (pixelFormat.flags & DDS_PF_NORMAL) {
                        if (cmp == Compression::BC5) {
                            ComputeNormalRG(rgbai);
                        } else if (cmp == Compression::DXT5) {
                            ComputeNormalAG(rgbai);
                        }
                    }

                    // Write the pixels into the destination image location,
                    // making sure to not go outside of image boundaries (BCn
                    // blocks always decode to 4x4 pixels, but output image
                    // might not be multiple of 4).
                    if (cmp == Compression::BC6HU
                        || cmp == Compression::BC6HS) {
                        // HDR formats: half
                        const uint16_t* src = rgbh;
                        uint16_t* dst       = (uint16_t*)rgba
                                        + channelCount
                                              * (size_t(width) * y + x);
                        for (int py = 0; py < kBlockSize && y + py < yend;
                             py++) {
                            int cols = std::min(kBlockSize, width - x);
                            memcpy(dst, src, cols * channelCount * 2);
                            src += kBlockSize * channelCount;
                            dst += channelCount * width;
                        }
                    } else {
                        // LDR formats: uint8
                        const uint8_t* src = rgbai;
                        uint8_t* dst       = rgba
                                       + channelCount * (size_t(width) * y + x);
                        for (int py = 0; py < kBlockSize && y + py < yend;
                             py++) {
                            int cols = std::min(kBlockSize, width - x);
                            memcpy(dst, src, cols * channelCount);
                            src += kBlockSize * channelCount;
                            dst += channelCount * width;
                        }
                    }
                }
            }
        },
        opt);
}

/// Gets the bitmasks required to extract the channels of a DXGI format.
/// Returns whether the DXGI format is supported.
/// Compressed formats BCn are not handled by this function.
///
static bool
GetDxgiFormatChannelMasks(uint32_t dxgiFormat, uint32_t masks[4])
{
    masks[0] = masks[1] = masks[2] = masks[3] = 0;

    switch (dxgiFormat) {
    case DDS_FORMAT_R16_UNORM: masks[0] = 0xFFFF; break;

    case DDS_FORMAT_R10G10B10A2_UNORM:
        masks[0] = 0x000003FF;
        masks[1] = 0x000FFC00;
        masks[2] = 0x3FF00000;
        masks[3] = 0xC0000000;
        break;

    case DDS_FORMAT_R8G8B8A8_UNORM:
    case DDS_FORMAT_R8G8B8A8_UNORM_SRGB:
        masks[0] = 0x000000FF;
        masks[1] = 0x0000FF00;
        masks[2] = 0x00FF0000;
        masks[3] = 0xFF000000;
        break;

    case DDS_FORMAT_B8G8R8A8_UNORM:
    case DDS_FORMAT_B8G8R8A8_UNORM_SRGB:
        masks[3] = 0xFF000000;
        OIIO_FALLTHROUGH;
    case DDS_FORMAT_B8G8R8X8_UNORM:
    case DDS_FORMAT_B8G8R8X8_UNORM_SRGB:
        masks[0] = 0x00FF0000;
        masks[1] = 0x0000FF00;
        masks[2] = 0x000000FF;
        break;

    default: return false;
    }

    return true;
}

/// Gets the bits-per-pixel of a DXGI format, or 0 if not supported.
/// Compressed formats BCn are not handled by this function.
///
static uint32_t
GetDxgiFormatBitsPerPixel(uint32_t dxgiFormat)
{
    switch (dxgiFormat) {
    case DDS_FORMAT_R16_UNORM: return 16;

    case DDS_FORMAT_R10G10B10A2_UNORM:
    case DDS_FORMAT_R8G8B8A8_UNORM:
    case DDS_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DDS_FORMAT_B8G8R8A8_UNORM:
    case DDS_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DDS_FORMAT_B8G8R8X8_UNORM:
    case DDS_FORMAT_B8G8R8X8_UNORM_SRGB: return 32;

    default: return 0;
    }
}


// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageInput*
dds_input_imageio_create()
{
    return new DDSInput;
}

OIIO_EXPORT int dds_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
dds_imageio_library_version()
{
    return nullptr;
}

OIIO_EXPORT const char* dds_input_extensions[] = { "dds", nullptr };

OIIO_PLUGIN_EXPORTS_END



bool
DDSInput::validate_signature(uint32_t signature)
{
    return signature == DDS_MAKE4CC('D', 'D', 'S', ' ');
}



bool
DDSInput::valid_file(Filesystem::IOProxy* ioproxy) const
{
    if (!ioproxy || ioproxy->mode() != Filesystem::IOProxy::Mode::Read)
        return false;

    uint32_t magic {};
    const size_t numRead = ioproxy->pread(&magic, sizeof(magic), 0);
    return numRead == sizeof(magic) && validate_signature(magic);
}



bool
DDSInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    ioproxy_retrieve_from_config(config);
    return open(name, newspec);
}



bool
DDSInput::open(const std::string& name, ImageSpec& newspec)
{
    m_filename = name;

    if (!ioproxy_use_or_open(name))
        return false;
    ioseek(0);

    static_assert(sizeof(dds_header) == 128, "dds header size does not match");
    if (!ioread(&m_dds, sizeof(m_dds), 1))
        return false;

    if (bigendian()) {
        // DDS files are little-endian
        // only swap values which are not flags or bitmasks
        swap_endian(&m_dds.size);
        swap_endian(&m_dds.height);
        swap_endian(&m_dds.width);
        swap_endian(&m_dds.pitch);
        swap_endian(&m_dds.depth);
        swap_endian(&m_dds.mipmaps);

        swap_endian(&m_dds.fmt.size);
        swap_endian(&m_dds.fmt.bpp);
    }

    /*std::cerr << "[dds] fourCC: " << ((char *)&m_dds.fourCC)[0]
                                  << ((char *)&m_dds.fourCC)[1]
                                  << ((char *)&m_dds.fourCC)[2]
                                  << ((char *)&m_dds.fourCC)[3]
                                  << " (" << m_dds.fourCC << ")\n";
    std::cerr << "[dds] size: " << m_dds.size << "\n";
    std::cerr << "[dds] flags: " << m_dds.flags << "\n";
    std::cerr << "[dds] pitch: " << m_dds.pitch << "\n";
    std::cerr << "[dds] width: " << m_dds.width << "\n";
    std::cerr << "[dds] height: " << m_dds.height << "\n";
    std::cerr << "[dds] depth: " << m_dds.depth << "\n";
    std::cerr << "[dds] mipmaps: " << m_dds.mipmaps << "\n";
    std::cerr << "[dds] fmt.size: " << m_dds.fmt.size << "\n";
    std::cerr << "[dds] fmt.flags: " << m_dds.fmt.flags << "\n";
    std::cerr << "[dds] fmt.fourCC: " << ((char *)&m_dds.fmt.fourCC)[0]
                                      << ((char *)&m_dds.fmt.fourCC)[1]
                                      << ((char *)&m_dds.fmt.fourCC)[2]
                                      << ((char *)&m_dds.fmt.fourCC)[3]
                                      << " (" << m_dds.fmt.fourCC << ")\n";
    std::cerr << "[dds] fmt.bpp: " << m_dds.fmt.bpp << "\n";
    std::cerr << "[dds] fmt.masks[0]: " << m_dds.fmt.masks[0] << "\n";
    std::cerr << "[dds] fmt.masks[1]: " << m_dds.fmt.masks[1] << "\n";
    std::cerr << "[dds] fmt.masks[2]: " << m_dds.fmt.masks[2] << "\n";
    std::cerr << "[dds] fmt.masks[3]: " << m_dds.fmt.masks[3] << "\n";
    std::cerr << "[dds] caps.flags1: " << m_dds.caps.flags1 << "\n";
    std::cerr << "[dds] caps.flags2: " << m_dds.caps.flags2 << "\n";*/

    // sanity checks - valid 4CC, correct struct sizes and flags which should
    // be always present, regardless of the image type, size etc., also check
    // for impossible flag combinations
    if (!validate_signature(m_dds.fourCC) || m_dds.size != 124
        || m_dds.fmt.size != 32 || !(m_dds.caps.flags1 & DDS_CAPS1_TEXTURE)
        || !(m_dds.flags & DDS_CAPS) || !(m_dds.flags & DDS_PIXELFORMAT)
        || (m_dds.caps.flags2 & DDS_CAPS2_VOLUME
            && !(m_dds.caps.flags1 & DDS_CAPS1_COMPLEX
                 && m_dds.flags & DDS_DEPTH))
        || (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP
            && !(m_dds.caps.flags1 & DDS_CAPS1_COMPLEX))) {
        errorfmt("Invalid DDS header, possibly corrupt file");
        return false;
    }

    // make sure all dimensions are > 0 and that we have at least one channel
    // (for uncompressed images)
    if (!(m_dds.flags & DDS_WIDTH) || !m_dds.width
        || !(m_dds.flags & DDS_HEIGHT) || !m_dds.height
        || ((m_dds.flags & DDS_DEPTH) && !m_dds.depth)
        || (!(m_dds.fmt.flags & DDS_PF_FOURCC)
            && !(m_dds.fmt.flags
                 & (DDS_PF_RGB | DDS_PF_LUMINANCE | DDS_PF_ALPHA
                    | DDS_PF_ALPHAONLY)))) {
        errorfmt("Image with no data");
        return false;
    }

    // read optional DX10 header
    if (m_dds.fmt.fourCC == DDS_4CC_DX10) {
        if (!ioread(&m_dx10, sizeof(m_dx10), 1))
            return false;

        /*std::cerr << "[dds:dx10] dxgiFormat: " << m_dx10.dxgiFormat << "\n";
        std::cerr << "[dds:dx10] resourceDimension: " << m_dx10.resourceDimension << "\n";
        std::cerr << "[dds:dx10] arraySize: " << m_dx10.arraySize << "\n";
        std::cerr << "[dds:dx10] miscFlag: " << m_dx10.miscFlag << "\n";
        std::cerr << "[dds:dx10] miscFlag2: " << m_dx10.miscFlag2 << "\n";*/
    }

    // validate the pixel format
    if (m_dds.fmt.flags & DDS_PF_FOURCC) {
        m_compression = Compression::None;
        switch (m_dds.fmt.fourCC) {
        case DDS_4CC_DXT1: m_compression = Compression::DXT1; break;
        case DDS_4CC_DXT2: m_compression = Compression::DXT2; break;
        case DDS_4CC_DXT3: m_compression = Compression::DXT3; break;
        case DDS_4CC_DXT4: m_compression = Compression::DXT4; break;
        case DDS_4CC_DXT5: m_compression = Compression::DXT5; break;
        case DDS_4CC_RXGB:
            m_compression = Compression::DXT5;
            m_dds.fmt.flags &= ~DDS_PF_NORMAL;
            break;
        case DDS_4CC_ATI1: m_compression = Compression::BC4; break;
        case DDS_4CC_ATI2: m_compression = Compression::BC5; break;
        case DDS_4CC_BC4U: m_compression = Compression::BC4; break;
        case DDS_4CC_BC5U: m_compression = Compression::BC5; break;
        case DDS_4CC_DX10: {
            switch (m_dx10.dxgiFormat) {
            case DDS_FORMAT_BC1_UNORM:
            case DDS_FORMAT_BC1_UNORM_SRGB:
                m_compression = Compression::DXT1;
                break;
            case DDS_FORMAT_BC2_UNORM:
            case DDS_FORMAT_BC2_UNORM_SRGB:
                m_compression = Compression::DXT3;
                break;
            case DDS_FORMAT_BC3_UNORM:
            case DDS_FORMAT_BC3_UNORM_SRGB:
                m_compression = Compression::DXT5;
                break;
            case DDS_FORMAT_BC4_UNORM: m_compression = Compression::BC4; break;
            case DDS_FORMAT_BC5_UNORM: m_compression = Compression::BC5; break;
            case DDS_FORMAT_BC6H_UF16:
                m_compression = Compression::BC6HU;
                break;
            case DDS_FORMAT_BC6H_SF16:
                m_compression = Compression::BC6HS;
                break;
            case DDS_FORMAT_BC7_UNORM:
            case DDS_FORMAT_BC7_UNORM_SRGB:
                m_compression = Compression::BC7;
                break;

            default:
                if (!GetDxgiFormatChannelMasks(m_dx10.dxgiFormat,
                                               m_dds.fmt.masks)) {
                    errorfmt("Unsupported DXGI format: {}", m_dx10.dxgiFormat);
                    return false;
                }
                break;
            }
        } break;
        default:
            errorfmt("Unsupported compression type: {}", m_dds.fmt.fourCC);
            return false;
        }
    }

    // treat BC5 as normal maps if global attribute is set
    if ((m_compression == Compression::BC5)
        && OIIO::get_int_attribute("dds:bc5normal")) {
        m_dds.fmt.flags |= DDS_PF_NORMAL;
    }

    // determine the number of channels we have
    if (m_compression != Compression::None) {
        m_nchans = GetChannelCount(m_compression,
                                   m_dds.fmt.flags & DDS_PF_NORMAL);
    } else if (m_dds.fmt.fourCC == DDS_4CC_DX10) {
        // uncompressed DXGI formats, calculate bytes per pixel and bit shifts
        m_Bpp    = (GetDxgiFormatBitsPerPixel(m_dx10.dxgiFormat) + 7) >> 3;
        m_nchans = 0;
        for (int i = 0; i < 4; ++i) {
            if (m_dds.fmt.masks[i] != 0) {
                // place channels sequentially
                m_dds.fmt.masks[m_nchans] = m_dds.fmt.masks[i];
                m_nchans++;
            }
        }

        for (int i = 0; i < m_nchans; ++i)
            calc_shifts(m_dds.fmt.masks[i], m_BitCounts[i], m_RightShifts[i]);
    } else {
        // also calculate bytes per pixel and the bit shifts
        m_Bpp = (m_dds.fmt.bpp + 7) >> 3;
        for (int i = 0; i < 4; ++i)
            calc_shifts(m_dds.fmt.masks[i], m_BitCounts[i], m_RightShifts[i]);
        m_nchans = 3;
        if (m_dds.fmt.flags & DDS_PF_LUMINANCE) {
            // we treat luminance as one channel;
            // move next channel (possible alpha) info
            // after it
            m_nchans           = 1;
            m_dds.fmt.masks[1] = m_dds.fmt.masks[3];
            m_BitCounts[1]     = m_BitCounts[3];
            m_RightShifts[1]   = m_RightShifts[3];
        } else if (m_dds.fmt.flags & DDS_PF_ALPHAONLY) {
            // alpha-only image; move alpha info
            // into the first slot
            m_nchans           = 1;
            m_dds.fmt.masks[0] = m_dds.fmt.masks[3];
            m_BitCounts[0]     = m_BitCounts[3];
            m_RightShifts[0]   = m_RightShifts[3];
        }
        if (m_dds.fmt.flags & DDS_PF_ALPHA)
            m_nchans++;
    }

    // fix depth, pitch and mipmaps for later use, if needed
    if (!(m_dds.fmt.flags & DDS_PF_FOURCC && m_dds.flags & DDS_PITCH))
        m_dds.pitch = m_dds.width * m_Bpp;
    if (!(m_dds.caps.flags2 & DDS_CAPS2_VOLUME))
        m_dds.depth = 1;
    if (!(m_dds.flags & DDS_MIPMAPCOUNT))
        m_dds.mipmaps = 1;
    // count cube map faces
    if (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP) {
        m_nfaces = 0;
        for (int flag = DDS_CAPS2_CUBEMAP_POSITIVEX;
             flag <= DDS_CAPS2_CUBEMAP_NEGATIVEZ; flag <<= 1) {
            if (m_dds.caps.flags2 & flag)
                m_nfaces++;
        }
    } else
        m_nfaces = 1;

    if (!seek_subimage(0, 0))
        return false;
    newspec = spec();
    return true;
}



inline void
DDSInput::calc_shifts(uint32_t mask, uint32_t& count, uint32_t& right)
{
    if (mask == 0) {
        count = right = 0;
        return;
    }

    int i;
    for (i = 0; i < 32; i++, mask >>= 1) {
        if (mask & 1)
            break;
    }
    right = i;

    for (i = 0; i < 32; i++, mask >>= 1) {
        if (!(mask & 1))
            break;
    }
    count = i;
}



// NOTE: This function has no sanity checks! It's a private method and relies
// on the input being correct and valid!
void
DDSInput::internal_seek_subimage(int cubeface, int miplevel, unsigned int& w,
                                 unsigned int& h, unsigned int& d)
{
    // early out for cubemaps that don't contain the requested face
    if (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP
        && !(m_dds.caps.flags2 & (DDS_CAPS2_CUBEMAP_POSITIVEX << cubeface))) {
        w = h = d = 0;
        return;
    }
    // we can easily calculate the offsets because both compressed and
    // uncompressed images have predictable length
    // calculate the offset; start with after the header
    unsigned int ofs = sizeof(dds_header);
    if (m_dds.fmt.fourCC == DDS_4CC_DX10)
        ofs += sizeof(dds_header_dx10);
    unsigned int len;
    // this loop is used to iterate over cube map sides, or run once in the
    // case of ordinary 2D or 3D images
    for (int j = 0; j <= cubeface; j++) {
        w = m_dds.width;
        h = m_dds.height;
        d = m_dds.depth;
        // skip subimages preceding the one we're seeking to
        // if we have no mipmaps, the modulo formula doesn't work and we
        // don't skip at all, so just add the offset and continue
        if (m_dds.mipmaps < 2) {
            if (j > 0) {
                if (m_compression != Compression::None)
                    len = GetStorageRequirements(w, h, m_compression);
                else
                    len = w * h * d * m_Bpp;
                ofs += len;
            }
            continue;
        }
        // On the target cube face seek to the selected mip level.  On previous faces
        // seek past all levels.
        int seekLevel = (j == cubeface) ? miplevel : m_dds.mipmaps;
        for (int i = 0; i < seekLevel; i++) {
            if (m_compression != Compression::None)
                len = GetStorageRequirements(w, h, m_compression);
            else
                len = w * h * d * m_Bpp;
            ofs += len;
            w >>= 1;
            if (!w)
                w = 1;
            h >>= 1;
            if (!h)
                h = 1;
            d >>= 1;
            if (!d)
                d = 1;
        }
    }
    // seek to the offset we've found
    ioseek(ofs, SEEK_SET);
}



bool
DDSInput::seek_subimage(int subimage, int miplevel)
{
    if (subimage != 0)
        return false;

    // early out
    if (subimage == current_subimage() && miplevel == current_miplevel()) {
        return true;
    }

    // don't seek if the image doesn't contain mipmaps, isn't 3D or a cube map,
    // and don't seek out of bounds
    if (miplevel < 0
        || (!(m_dds.caps.flags1 & DDS_CAPS1_COMPLEX) && miplevel != 0)
        || (unsigned int)miplevel >= m_dds.mipmaps)
        return false;

    // clear buffer so that readimage is called
    m_buf.clear();

    // for cube maps, the seek will be performed when reading a tile instead
    unsigned int w = 0, h = 0, d = 0;
    TypeDesc::BASETYPE basetype = GetBaseType(m_compression);
    if (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP) {
        // calc sizes separately for cube maps
        w = m_dds.width;
        h = m_dds.height;
        d = m_dds.depth;
        for (int i = 1; i < miplevel; i++) {
            w >>= 1;
            if (w < 1)
                w = 1;
            h >>= 1;
            if (h < 1)
                h = 1;
            d >>= 1;
            if (d < 1)
                d = 1;
        }
        // create imagespec for the 3x2 cube map layout
#ifdef DDS_3X2_CUBE_MAP_LAYOUT
        m_spec = ImageSpec(w * 3, h * 2, m_nchans, basetype);
#else   // 1x6 layout
        m_spec = ImageSpec(w, h * 6, m_nchans, basetype);
#endif  // DDS_3X2_CUBE_MAP_LAYOUT
        m_spec.depth      = d;
        m_spec.tile_width = m_spec.full_width = w;
        m_spec.tile_height = m_spec.full_height = h;
        m_spec.tile_depth = m_spec.full_depth = d;
    } else {
        internal_seek_subimage(0, miplevel, w, h, d);
        // create imagespec
        m_spec       = ImageSpec(w, h, m_nchans, basetype);
        m_spec.depth = d;
    }

    // fill the imagespec
    if (m_compression != Compression::None) {
        const char* str = nullptr;
        switch (m_compression) {
        case Compression::None: break;
        case Compression::DXT1: str = "DXT1"; break;
        case Compression::DXT2: str = "DXT2"; break;
        case Compression::DXT3: str = "DXT3"; break;
        case Compression::DXT4: str = "DXT4"; break;
        case Compression::DXT5: str = "DXT5"; break;
        case Compression::BC4: str = "BC4"; break;
        case Compression::BC5: str = "BC5"; break;
        case Compression::BC6HU: str = "BC6HU"; break;
        case Compression::BC6HS: str = "BC6HS"; break;
        case Compression::BC7: str = "BC7"; break;
        }
        if (str != nullptr)
            m_spec.attribute("compression", str);
    }

    uint32_t bpp = 0;
    if (m_dds.fmt.bpp
        && (m_dds.fmt.flags
            & (DDS_PF_RGB | DDS_PF_LUMINANCE | DDS_PF_YUV | DDS_PF_ALPHAONLY))) {
        if (m_dds.fmt.bpp != 8 && m_dds.fmt.bpp != 16 && m_dds.fmt.bpp != 24
            && m_dds.fmt.bpp != 32) {
            errorfmt(
                "Unsupported DDS bit depth: {} (maybe it's a corrupted file?)",
                m_dds.fmt.bpp);
            return false;
        }
        bpp = m_dds.fmt.bpp;
    } else if (m_dds.fmt.fourCC == DDS_4CC_DX10) {
        bpp = GetDxgiFormatBitsPerPixel(m_dx10.dxgiFormat);
    }

    if (bpp != 0)
        m_spec.attribute("oiio:BitsPerSample", bpp);

    const char* colorspace = nullptr;

    if (m_dds.fmt.fourCC == DDS_4CC_DX10) {
        switch (m_dx10.dxgiFormat) {
        case DDS_FORMAT_BC1_UNORM_SRGB:
        case DDS_FORMAT_BC2_UNORM_SRGB:
        case DDS_FORMAT_BC3_UNORM_SRGB:
        case DDS_FORMAT_BC7_UNORM_SRGB:
        case DDS_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DDS_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DDS_FORMAT_B8G8R8X8_UNORM_SRGB: colorspace = "sRGB"; break;
        }
    }

    // linear color space for HDR-ish images
    if (colorspace == nullptr
        && (basetype == TypeDesc::HALF || basetype == TypeDesc::FLOAT))
        colorspace = "lin_rec709";

    m_spec.set_colorspace(colorspace);

    m_spec.default_channel_names();
    // Special case: if a 2-channel DDS RG or YA?
    if (m_nchans == 2 && (m_dds.fmt.flags & DDS_PF_LUMINANCE)
        && (m_dds.fmt.flags & DDS_PF_ALPHA)) {
        m_spec.channelnames[0] = "Y";
        m_spec.channelnames[1] = "A";
    }

    // detect texture type
    if (m_dds.caps.flags2 & DDS_CAPS2_VOLUME) {
        m_spec.attribute("textureformat", "Volume Texture");
    } else if (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP) {
        m_spec.attribute("textureformat", "CubeFace Environment");
        // check available cube map sides
        std::string sides = "";
        if (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP_POSITIVEX)
            sides += "+x";
        if (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP_NEGATIVEX) {
            if (sides.size())
                sides += " ";
            sides += "-x";
        }
        if (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP_POSITIVEY) {
            if (sides.size())
                sides += " ";
            sides += "+y";
        }
        if (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP_NEGATIVEY) {
            if (sides.size())
                sides += " ";
            sides += "-y";
        }
        if (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP_POSITIVEZ) {
            if (sides.size())
                sides += " ";
            sides += "+z";
        }
        if (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP_NEGATIVEZ) {
            if (sides.size())
                sides += " ";
            sides += "-z";
        }
        m_spec.attribute("dds:CubeMapSides", sides);
    } else {
        m_spec.attribute("textureformat", "Plain Texture");
    }

    m_subimage = subimage;
    m_miplevel = miplevel;
    return true;
}

bool
DDSInput::internal_readimg(unsigned char* dst, int w, int h, int d)
{
    if (m_compression != Compression::None) {
        // compressed image
        // create source buffer
        size_t bufsize = GetStorageRequirements(w, h, m_compression);
        std::unique_ptr<uint8_t[]> tmp(new uint8_t[bufsize]);
        // load image into buffer
        if (!ioread(tmp.get(), bufsize, 1))
            return false;
        // decompress image
        DecompressImage(dst, w, h, tmp.get(), m_compression, m_dds.fmt,
                        threads());
        tmp.reset();
        // correct pre-multiplied alpha, if necessary
        if (m_compression == Compression::DXT2
            || m_compression == Compression::DXT4) {
            int k;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    k = (y * w + x) * 4;
                    if (dst[k + 3]) {
                        dst[k + 0] = (unsigned char)((int)dst[k + 0] * 255
                                                     / (int)dst[k + 3]);
                        dst[k + 1] = (unsigned char)((int)dst[k + 1] * 255
                                                     / (int)dst[k + 3]);
                        dst[k + 2] = (unsigned char)((int)dst[k + 2] * 255
                                                     / (int)dst[k + 3]);
                    }
                }
            }
        }
    } else {
        // uncompressed image:
        // check if we can just directly copy pixels without any processing
        bool direct = false;
        if (m_spec.nchannels == m_Bpp) {
            direct = true;
            for (int ch = 0; ch < m_spec.nchannels; ++ch) {
                if ((m_dds.fmt.masks[ch] != (0xFFu << (ch * 8)))
                    || (m_RightShifts[ch] != uint32_t(ch * 8))
                    || (m_BitCounts[ch] != 8u)) {
                    direct = false;
                    break;
                }
            }
        }
        if (direct) {
            return ioread(dst, w * m_Bpp, h);
        }

        std::unique_ptr<uint8_t[]> tmp(new uint8_t[w * m_Bpp]);
        for (int z = 0; z < d; z++) {
            for (int y = 0; y < h; y++) {
                if (!ioread(tmp.get(), w, m_Bpp))
                    return false;
                size_t k = (z * h * w + y * w) * m_spec.nchannels;
                for (int x = 0; x < w; x++, k += m_spec.nchannels) {
                    uint32_t pixel = 0;
                    memcpy(&pixel, tmp.get() + x * m_Bpp, m_Bpp);
                    for (int ch = 0; ch < m_spec.nchannels; ++ch) {
                        dst[k + ch]
                            = bit_range_convert((pixel & m_dds.fmt.masks[ch])
                                                    >> m_RightShifts[ch],
                                                m_BitCounts[ch], 8);
                    }
                }
            }
        }
    }
    return true;
}



bool
DDSInput::readimg_scanlines()
{
    //std::cerr << "[dds] readimg: " << ftell() << "\n";
    // resize destination buffer
    m_buf.resize(m_spec.scanline_bytes() * m_spec.height * m_spec.depth
                 /*/ (1 << m_miplevel)*/);

    return internal_readimg(&m_buf[0], m_spec.width, m_spec.height,
                            m_spec.depth);
}



bool
DDSInput::readimg_tiles()
{
    // resize destination buffer
    OIIO_ASSERT(m_buf.size() >= m_spec.tile_bytes());
    return internal_readimg(&m_buf[0], m_spec.tile_width, m_spec.tile_height,
                            m_spec.tile_depth);
}



bool
DDSInput::close()
{
    init();  // Reset to initial state
    return true;
}



bool
DDSInput::read_native_scanline(int subimage, int miplevel, int y, int z,
                               void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;

    // don't proceed if a cube map - use tiles then instead
    if (m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP)
        return false;
    if (m_buf.empty())
        readimg_scanlines();

    size_t size = spec().scanline_bytes();
    memcpy(data, &m_buf[0] + z * m_spec.height * size + y * size, size);
    return true;
}



bool
DDSInput::read_native_tile(int subimage, int miplevel, int x, int y, int z,
                           void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;

    // static ints to keep track of the current cube face and re-seek and
    // re-read face
    static int lastx = -1, lasty = -1, lastz = -1;
    // don't proceed if not a cube map - use scanlines then instead
    if (!(m_dds.caps.flags2 & DDS_CAPS2_CUBEMAP))
        return false;
    // make sure we get the right dimensions
    if (x % m_spec.tile_width || y % m_spec.tile_height
        || z % m_spec.tile_width)
        return false;
    if (m_buf.empty() || x != lastx || y != lasty || z != lastz) {
        lastx          = x;
        lasty          = y;
        lastz          = z;
        unsigned int w = 0, h = 0, d = 0;
#ifdef DDS_3X2_CUBE_MAP_LAYOUT
        internal_seek_subimage(((x / m_spec.tile_width) << 1)
                                   + y / m_spec.tile_height,
                               m_miplevel, w, h, d);
#else                                       // 1x6 layout
        internal_seek_subimage(y / m_spec.tile_height, m_miplevel, w, h, d);
#endif                                      // DDS_3X2_CUBE_MAP_LAYOUT
        m_buf.resize(m_spec.tile_bytes());  // resize destination buffer
        if (!w && !h && !d)  // face not present in file, black-pad the image
            memset(&m_buf[0], 0, m_spec.tile_bytes());
        else
            readimg_tiles();
    }

    memcpy(data, &m_buf[0], m_spec.tile_bytes());
    return true;
}

OIIO_PLUGIN_NAMESPACE_END
