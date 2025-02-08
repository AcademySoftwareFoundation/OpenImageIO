// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>

#include "bmp_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace bmp_pvt;


class BmpInput final : public ImageInput {
public:
    BmpInput() { init(); }
    ~BmpInput() override { close(); }
    const char* format_name(void) const override { return "bmp"; }
    int supports(string_view feature) const override
    {
        return feature == "ioproxy";
    }
    bool valid_file(Filesystem::IOProxy* ioproxy) const override;
    bool open(const std::string& name, ImageSpec& newspec) override;
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    bool close(void) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;

private:
    int64_t m_padded_scanline_size;
    int m_pad_size;
    bmp_pvt::BmpFileHeader m_bmp_header;
    bmp_pvt::DibInformationHeader m_dib_header;
    std::string m_filename;
    std::vector<bmp_pvt::color_table> m_colortable;
    std::vector<unsigned char> fscanline;       // temp space: read from file
    std::vector<unsigned char> m_uncompressed;  // uncompressed palette image
    uint32_t m_right_shifts[3];
    uint32_t m_bit_counts[3];
    bool m_allgray;

    void init(void)
    {
        m_padded_scanline_size = 0;
        m_pad_size             = 0;
        m_filename.clear();
        m_colortable.clear();
        m_allgray = false;
        fscanline.shrink_to_fit();
        m_uncompressed.shrink_to_fit();
        ioproxy_clear();
    }

    bool read_color_table();
    bool color_table_is_all_gray();
    bool read_rle_image();

    bool ioeof() { return size_t(ioproxy()->tell()) == ioproxy()->size(); }

    // Safe, clamped access to color table
    const bmp_pvt::color_table& colortable(int i)
    {
        return m_colortable[clamp(i, 0, int(m_colortable.size() - 1))];
    }
};



// Obligatory material to make this a recognizable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int bmp_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
bmp_imageio_library_version()
{
    return nullptr;
}

OIIO_EXPORT ImageInput*
bmp_input_imageio_create()
{
    return new BmpInput;
}

OIIO_EXPORT const char* bmp_input_extensions[] = { "bmp", "dib", nullptr };

OIIO_PLUGIN_EXPORTS_END


bool
BmpInput::valid_file(Filesystem::IOProxy* ioproxy) const
{
    if (!ioproxy || ioproxy->mode() != Filesystem::IOProxy::Mode::Read)
        return false;

    bmp_pvt::BmpFileHeader header;
    return header.read_header(ioproxy) && header.isBmp();
}



bool
BmpInput::open(const std::string& name, ImageSpec& newspec)
{
    ImageSpec emptyconfig;
    return open(name, newspec, emptyconfig);
}

inline void
calc_shifts(uint32_t mask, uint32_t& count, uint32_t& right)
{
    if (mask == 0) {
        count = right = 0;
        return;
    }

    uint32_t i;
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



bool
BmpInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    // saving 'name' for later use
    m_filename = name;

    // BMP cannot be 1-channel, but config hint "bmp:monochrome_detect" is a
    // hint to try to detect when all palette entries are gray and pretend
    // that it's a 1-channel image to allow the calling app to save memory
    // and time. It does this by default, but setting the hint to 0 turns
    // this behavior off.
    bool monodetect = config["bmp:monochrome_detect"].get<int>(1);

    ioproxy_retrieve_from_config(config);
    if (!ioproxy_use_or_open(name))
        return false;
    ioseek(0);

    // we read header of the file that we think is BMP file
    if (!m_bmp_header.read_header(ioproxy())) {
        errorfmt("\"{}\": wrong bmp header size", name);
        close();
        return false;
    }
    if (!m_bmp_header.isBmp()) {
        errorfmt("\"{}\" is not a BMP file, magic number doesn't match", name);
        close();
        return false;
    }
    // Strutil::print(
    //     "Header: magic={:x} {}{} fsize={} res1={} res2={} offset={}\n",
    //     m_bmp_header.magic, char(m_bmp_header.magic & 0xff),
    //     char(m_bmp_header.magic >> 8), m_bmp_header.fsize, m_bmp_header.res1,
    //     m_bmp_header.res2, m_bmp_header.offset);
    if (!m_dib_header.read_header(ioproxy())) {
        errorfmt("\"{}\": wrong bitmap header size", name);
        close();
        return false;
    }
    // Strutil::print(
    //     "Header: size={}(0x{:02x}) width={} height={} cplanes={} bpp={}\n",
    //     m_dib_header.size, m_dib_header.size, m_dib_header.width,
    //     m_dib_header.height, m_dib_header.cplanes, m_dib_header.bpp);

    const int nchannels = (m_dib_header.bpp == 32) ? 4 : 3;
    const int height    = (m_dib_header.height >= 0) ? m_dib_header.height
                                                     : -m_dib_header.height;
    m_spec = ImageSpec(m_dib_header.width, height, nchannels, TypeDesc::UINT8);
    if (m_dib_header.hres > 0 && m_dib_header.vres > 0) {
        m_spec.attribute("XResolution", (int)m_dib_header.hres);
        m_spec.attribute("YResolution", (int)m_dib_header.vres);
        m_spec.attribute("ResolutionUnit", "m");
    }
    if (m_spec.width < 1 || m_spec.height < 1 || m_spec.nchannels < 1
        || m_spec.image_bytes() < 1
        || m_spec.image_pixels() > std::numeric_limits<uint32_t>::max()) {
        errorfmt(
            "Invalid image size {} x {} ({} chans, {}), is likely corrupted",
            m_spec.width, m_spec.height, m_spec.nchannels, m_spec.format);
        close();
        return false;
    }

    // Compute channel shifts & masks (only relevant for 16bpp case)
    if (m_dib_header.red_mask == 0 || m_dib_header.green_mask == 0
        || m_dib_header.blue_mask == 0) {
        m_dib_header.red_mask   = 0b111110000000000;
        m_dib_header.green_mask = 0b000001111100000;
        m_dib_header.blue_mask  = 0b000000000011111;
    }
    calc_shifts(m_dib_header.red_mask, m_bit_counts[0], m_right_shifts[0]);
    calc_shifts(m_dib_header.green_mask, m_bit_counts[1], m_right_shifts[1]);
    calc_shifts(m_dib_header.blue_mask, m_bit_counts[2], m_right_shifts[2]);

    // computing size of one scanline - this is the size of one scanline that
    // is stored in the file, not in the memory
    int swidth = 0;
    switch (m_dib_header.bpp) {
    case 32:
    case 24:
        m_padded_scanline_size = ((m_spec.width * m_spec.nchannels) + 3) & ~3;
        break;
    case 16:
        m_padded_scanline_size = ((m_spec.width << 1) + 3) & ~3;
        m_spec.attribute("oiio:BitsPerSample", m_bit_counts[0]);
        break;
    case 8:
        m_padded_scanline_size = (m_spec.width + 3) & ~3;
        if (!read_color_table())
            return false;
        m_allgray = monodetect && color_table_is_all_gray();
        if (m_allgray) {
            m_spec.nchannels = 1;  // make it look like a 1-channel image
            m_spec.default_channel_names();
        }
        break;
    case 4:
        swidth                 = (m_spec.width + 1) / 2;
        m_padded_scanline_size = (swidth + 3) & ~3;
        if (!read_color_table())
            return false;
        break;
    case 1:
        swidth                 = (m_spec.width + 7) / 8;
        m_padded_scanline_size = (swidth + 3) & ~3;
        if (!read_color_table())
            return false;
        break;
    default:
        errorfmt("Unsupported BMP bit depth: {}", m_dib_header.bpp);
        return false;
    }
    if (m_dib_header.bpp <= 16)
        m_spec.attribute("bmp:bitsperpixel", m_dib_header.bpp);
    switch (m_dib_header.size) {
    case OS2_V1: m_spec.attribute("bmp:version", 1); break;
    case WINDOWS_V3: m_spec.attribute("bmp:version", 3); break;
    case WINDOWS_V4: m_spec.attribute("bmp:version", 4); break;
    case WINDOWS_V5: m_spec.attribute("bmp:version", 5); break;
    }

    // Default presumption is that a BMP file is meant to look reasonable on a
    // display, so assume it's sRGB. This is not really correct -- see the
    // comments below.
    m_spec.attribute("oiio:ColorSpace", "sRGB");
#if 0
    if (m_dib_header.size >= WINDOWS_V4
        && m_dib_header.cs_type == CSType::CalibratedRGB) {
        // FIXME: V4 and newer BMP files have color primary information, but
        // we currently ignore it and presume sRGB. I don't know how
        // frequently the color primaries are reliable or if anybody cares for
        // this ancient format. We may come back to this later.
    }
    if (m_dib_header.size >= WINDOWS_V4
        && m_dib_header.cs_type == CSType::DeviceDependentCMYK) {
        // FIXME: I've never encountered a BMP file that holds CMYK data. If
        // this is a problem for people, we can return to fix this later.
    }
#endif

    // Bite the bullet and uncompress now, for simplicity
    if (m_dib_header.compression == RLE4_COMPRESSION
        || m_dib_header.compression == RLE8_COMPRESSION) {
        if (!read_rle_image()) {
            errorfmt("BMP error reading rle-compressed image");
            close();
            return false;
        }
    }

    newspec = m_spec;
    return true;
}



bool
BmpInput::read_rle_image()
{
    int rletype = m_dib_header.compression == RLE4_COMPRESSION ? 4 : 8;
    m_spec.attribute("compression", rletype == 4 ? "rle4" : "rle8");
    m_uncompressed.clear();
    m_uncompressed.resize(m_spec.image_pixels());
    // Note: the clear+resize zeroes out the buffer
    bool ok = true;
    int y = 0, x = 0;
    while (ok) {
        // Strutil::print("currently at {},{}\n", x, y);
        unsigned char rle_pair[2];
        if (!ioread(rle_pair, 2)) {
            ok = false;
            // Strutil::print("hit end of file at {},{}\n", x, y);
            break;
        }
        if (y >= m_spec.height) {  // out of y bounds
            errorfmt(
                "BMP might be corrupted, it is referencing an out-of-bounds pixel coordinate ({},{})",
                x, y);
            ok = false;
            break;
        }
        int npixels = rle_pair[0];
        int value   = rle_pair[1];
        if (npixels == 0 && value == 0) {
            // [0,0] is end of line marker
            x = 0;
            ++y;
            // Strutil::print("end of line, moving to {},{}\n", x, y);
        } else if (npixels == 0 && value == 1) {
            // [0,1] is end of bitmap marker
            // Strutil::print("end of bitmap\n");
            break;
        } else if (npixels == 0 && value == 2) {
            // [0,2] is a "delta" -- two more bytes reposition the
            // current pixel position that we're reading.
            unsigned char offset[2];
            ok &= ioread(offset, 2);
            x += offset[0];
            y += offset[1];
            // Strutil::print("offset by {:d},{:d} to {},{}\n", offset[0],
            //                offset[1], x, y);
        } else if (npixels == 0) {
            // [0,n>2] is an "absolute" run of pixel data.
            // n is the number of pixel indices that follow, but note
            // that it pads to word size.
            npixels    = value;
            int nbytes = (rletype == 4)
                             ? round_to_multiple((npixels + 1) / 2, 2)
                             : round_to_multiple(npixels, 2);
            // Strutil::print("rle of {} pixels at {},{}\n", npixels, x, y);
            unsigned char absolute[256];
            ok &= ioread(absolute, nbytes);
            for (int i = 0; i < npixels; ++i, ++x) {
                if (rletype == 4)
                    value = (i & 1) ? (absolute[i / 2] & 0x0f)
                                    : (absolute[i / 2] >> 4);
                else
                    value = absolute[i];
                if (x < m_spec.width)
                    m_uncompressed[y * m_spec.width + x] = value;
            }
        } else {
            // [n>0,p] is a run of n pixels.
            // Strutil::print("direct read {} pixels at {},{}\n", npixels, x, y);
            for (int i = 0; i < npixels; ++i, ++x) {
                int v;
                if (rletype == 4)
                    v = (i & 1) ? (value & 0x0f) : (value >> 4);
                else
                    v = value;
                if (x < m_spec.width)
                    m_uncompressed[y * m_spec.width + x] = v;
            }
        }
    }
    return ok;
}



bool
BmpInput::read_native_scanline(int subimage, int miplevel, int y, int /*z*/,
                               void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;

    if (y < 0 || y > m_spec.height)
        return false;

    size_t scanline_bytes = m_spec.scanline_bytes();
    uint8_t* mscanline    = (uint8_t*)data;
    if (m_dib_header.compression == RLE4_COMPRESSION
        || m_dib_header.compression == RLE8_COMPRESSION) {
        for (int x = 0; x < m_spec.width; ++x) {
            int p = m_uncompressed[(m_spec.height - 1 - y) * m_spec.width + x];
            auto& c              = colortable(p);
            mscanline[3 * x]     = c.r;
            mscanline[3 * x + 1] = c.g;
            mscanline[3 * x + 2] = c.b;
        }
        return true;
    }

    // if the height is positive scanlines are stored bottom-up
    if (m_dib_header.height >= 0)
        y = m_spec.height - y - 1;
    const int64_t scanline_off = y * m_padded_scanline_size;

    fscanline.resize(m_padded_scanline_size);
    ioseek(m_bmp_header.offset + scanline_off);
    if (!ioread(fscanline.data(), m_padded_scanline_size)) {
        return false;  // Read failed
    }

    // in each case we process only first m_spec.scanline_bytes () bytes
    // as only they contain information about pixels. The rest are just
    // because scanline size have to be 32-bit boundary
    if (m_dib_header.bpp == 24 || m_dib_header.bpp == 32) {
        for (unsigned int i = 0; i < m_spec.scanline_bytes();
             i += m_spec.nchannels)
            std::swap(fscanline[i], fscanline[i + 2]);
        memcpy(data, fscanline.data(), m_spec.scanline_bytes());
        return true;
    }

    if (m_dib_header.bpp == 16) {
        for (unsigned int i = 0, j = 0; j < scanline_bytes; i += 2, j += 3) {
            uint16_t pixel = *(uint16_t*)&fscanline[i];
            mscanline[j + 0]
                = (uint8_t)bit_range_convert((pixel & m_dib_header.red_mask)
                                                 >> m_right_shifts[0],
                                             m_bit_counts[0], 8);
            mscanline[j + 1]
                = (uint8_t)bit_range_convert((pixel & m_dib_header.green_mask)
                                                 >> m_right_shifts[1],
                                             m_bit_counts[1], 8);
            mscanline[j + 2]
                = (uint8_t)bit_range_convert((pixel & m_dib_header.blue_mask)
                                                 >> m_right_shifts[2],
                                             m_bit_counts[2], 8);
        }
    }
    if (m_dib_header.bpp == 8) {
        if (m_allgray) {
            // Keep it as 1-channel image because all colors are gray
            for (unsigned int i = 0; i < scanline_bytes; ++i) {
                mscanline[i] = colortable(fscanline[i]).r;
            }
        } else {
            // Expand palette image into 3-channel RGB (existing code)
            for (unsigned int i = 0, j = 0; j < scanline_bytes; ++i, j += 3) {
                auto& c          = colortable(fscanline[i]);
                mscanline[j]     = c.r;
                mscanline[j + 1] = c.g;
                mscanline[j + 2] = c.b;
            }
        }
    }
    if (m_dib_header.bpp == 4) {
        for (unsigned int i = 0, j = 0; j < scanline_bytes; ++i, j += 6) {
            uint8_t mask = 0xF0;
            {
                auto& c          = colortable((fscanline[i] & mask) >> 4);
                mscanline[j]     = c.r;
                mscanline[j + 1] = c.g;
                mscanline[j + 2] = c.b;
            }
            if (j + 3 >= scanline_bytes)
                break;
            mask = 0x0F;
            {
                auto& c          = colortable(fscanline[i] & mask);
                mscanline[j + 3] = c.r;
                mscanline[j + 4] = c.g;
                mscanline[j + 5] = c.b;
            }
        }
    }
    if (m_dib_header.bpp == 1) {
        for (int64_t i = 0, k = 0; i < m_padded_scanline_size; ++i) {
            for (int j = 7; j >= 0; --j, k += 3) {
                if (size_t(k + 2) >= scanline_bytes)
                    break;
                int index = 0;
                if (fscanline[i] & (1 << j))
                    index = 1;
                auto& c          = colortable(index);
                mscanline[k]     = c.r;
                mscanline[k + 1] = c.g;
                mscanline[k + 2] = c.b;
            }
        }
    }
    return true;
}



bool
BmpInput::close(void)
{
    init();
    return true;
}



bool
BmpInput::read_color_table(void)
{
    // size of color table is defined  by m_dib_header.cpalete
    // if this field is 0 - color table has max colors:
    // pow(2, m_dib_header.cpalete) otherwise color table have
    // m_dib_header.cpalete entries
    if (m_dib_header.cpalete < 0
        || m_dib_header.cpalete > (1 << m_dib_header.bpp)) {
        errorfmt("Possible corrupted header, invalid palette size");
        return false;
    }
    const int32_t colors = (m_dib_header.cpalete) ? m_dib_header.cpalete
                                                  : 1 << m_dib_header.bpp;
    size_t entry_size    = 4;
    // if the file is OS V2 bitmap color table entry has only 3 bytes, not four
    if (m_dib_header.size == OS2_V1)
        entry_size = 3;
    m_colortable.resize(colors);
    for (int i = 0; i < colors; i++) {
        if (!ioread(&m_colortable[i], entry_size)) {
            if (ioeof())
                errorfmt(
                    "Hit end of file unexpectedly while reading color table on color {}/{})",
                    i, colors);
            else
                errorfmt("read error while reading color table");
            return false;  // Read failed
        }
    }
    return true;  // ok
}



bool
BmpInput::color_table_is_all_gray(void)
{
    size_t ncolors = m_colortable.size();
    for (size_t i = 0; i < ncolors; i++) {
        color_table& color = m_colortable[i];
        if (color.b != color.g || color.g != color.r)
            return false;
    }
    return true;
}

OIIO_PLUGIN_NAMESPACE_END
