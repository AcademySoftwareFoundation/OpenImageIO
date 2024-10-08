// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>

#include "bmp_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace bmp_pvt;


class BmpOutput final : public ImageOutput {
public:
    BmpOutput() { init(); }
    ~BmpOutput() override { close(); }
    const char* format_name(void) const override { return "bmp"; }
    int supports(string_view feature) const override;
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode) override;
    bool close(void) override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;

private:
    int64_t m_padded_scanline_size;
    std::string m_filename;
    bmp_pvt::BmpFileHeader m_bmp_header;
    bmp_pvt::DibInformationHeader m_dib_header;
    int64_t m_image_start;
    unsigned int m_dither;
    std::vector<unsigned char> m_tilebuffer;
    std::vector<unsigned char> m_scratch;
    std::vector<unsigned char> m_buf;  // more tmp space for write_scanline

    void init(void)
    {
        m_padded_scanline_size = 0;
        m_filename.clear();
        ioproxy_clear();
    }

    void create_and_write_file_header(void);

    void create_and_write_bitmap_header(void);
};



// Obligatory material to make this a recognizable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
bmp_output_imageio_create()
{
    return new BmpOutput;
}

OIIO_EXPORT const char* bmp_output_extensions[] = { "bmp", nullptr };

OIIO_PLUGIN_EXPORTS_END



int
BmpOutput::supports(string_view feature) const
{
    return (feature == "alpha" || feature == "ioproxy");
}



bool
BmpOutput::open(const std::string& name, const ImageSpec& spec, OpenMode mode)
{
    if (!check_open(mode, spec, { 0, 65535, 0, 65535, 0, 1, 0, 4 },
                    uint64_t(OpenChecks::Disallow2Channel)))
        return false;

    m_filename = name;

    // Only support 8 bit channels for now.
    m_spec.set_format(TypeDesc::UINT8);
    m_dither = m_spec.get_int_attribute("oiio:dither", 0);

    int64_t file_size = m_spec.image_bytes() + BMP_HEADER_SIZE + WINDOWS_V3;
    if (file_size >= int64_t(1) << 32) {
        errorfmt("{} does not support files over 4GB in size\n", format_name());
        return false;
    }

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    // Scanline size is rounded up to align to 4-byte boundary
    m_padded_scanline_size = round_to_multiple(m_spec.scanline_bytes(), 4);

    create_and_write_file_header();
    create_and_write_bitmap_header();

    m_image_start = iotell();

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize(m_spec.image_bytes());

    return true;
}



bool
BmpOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    if (!ioproxy_opened()) {
        errorfmt("write_scanline called but file is not open.");
        return false;
    }

    if (y > m_spec.y + m_spec.height) {
        errorfmt("Attempt to write too many scanlines to {}", m_filename);
        close();
        return false;
    }

    y -= m_spec.y;
    if (m_spec.width >= 0)
        y = (m_spec.height - y - 1);
    int64_t scanline_off = y * m_padded_scanline_size;
    ioseek(m_image_start + scanline_off);

    m_scratch.clear();
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);
    m_buf.assign((const unsigned char*)data,
                 (const unsigned char*)data + m_spec.scanline_bytes());
    m_buf.resize(m_padded_scanline_size, 0);  // pad with zeroes if needed

    // Swap RGB pixels into BGR format
    if (m_spec.nchannels >= 3)
        for (int i = 0, iend = m_buf.size() - 2; i < iend;
             i += m_spec.nchannels)
            std::swap(m_buf[i], m_buf[i + 2]);

    return iowrite(&m_buf[0], m_buf.size());
}



bool
BmpOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    if (!ioproxy_opened()) {
        errorfmt("write_tile called but file is not open.");
        return false;
    }

    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, m_tilebuffer.data());
}



bool
BmpOutput::close(void)
{
    if (!ioproxy_opened()) {  // already closed
        init();
        return true;
    }

    bool ok = true;
    if (m_spec.tile_width && m_tilebuffer.size()) {
        // Handle tile emulation -- output the buffered pixels
        OIIO_DASSERT(m_tilebuffer.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, m_tilebuffer.data());
        std::vector<unsigned char>().swap(m_tilebuffer);
    }

    init();
    return ok;
}


void
BmpOutput::create_and_write_file_header(void)
{
    m_bmp_header.magic = MAGIC_BM;
    int64_t data_size  = m_padded_scanline_size * m_spec.height;
    int palettesize    = (m_spec.nchannels == 1) ? 4 * 256 : 0;
    int64_t file_size  = data_size + BMP_HEADER_SIZE + WINDOWS_V3 + palettesize;
    m_bmp_header.fsize = file_size;
    m_bmp_header.res1  = 0;
    m_bmp_header.res2  = 0;
    m_bmp_header.offset = BMP_HEADER_SIZE + WINDOWS_V3 + palettesize;

    m_bmp_header.write_header(ioproxy());
}



void
BmpOutput::create_and_write_bitmap_header(void)
{
    m_dib_header.size        = WINDOWS_V3;
    m_dib_header.width       = m_spec.width;
    m_dib_header.height      = m_spec.height;
    m_dib_header.cplanes     = 1;
    m_dib_header.compression = NO_COMPRESSION;

    if (m_spec.nchannels == 1) {
        // Special case -- write a 1-channel image as a gray palette
        m_dib_header.bpp       = 8;
        m_dib_header.cpalete   = 256;
        m_dib_header.important = 256;
    } else {
        m_dib_header.bpp       = m_spec.nchannels * 8;
        m_dib_header.cpalete   = 0;
        m_dib_header.important = 0;
    }

    m_dib_header.isize = int32_t(m_spec.image_pixels());
    m_dib_header.hres  = 0;
    m_dib_header.vres  = 0;

    string_view res_units = m_spec.get_string_attribute("ResolutionUnit");
    if (Strutil::iequals(res_units, "m")
        || Strutil::iequals(res_units, "pixel per meter")) {
        m_dib_header.hres = m_spec.get_int_attribute("XResolution");
        m_dib_header.vres = m_spec.get_int_attribute("YResolution");
    }

    m_dib_header.write_header(ioproxy());

    // Write palette, if there is one. This is only used for grayscale
    // images, and the palette is just the 256 possible gray values.
    for (int i = 0; i < m_dib_header.cpalete; ++i) {
        unsigned char val[4] = { uint8_t(i), uint8_t(i), uint8_t(i), 255 };
        iowrite(&val, 4);
    }
}

OIIO_PLUGIN_NAMESPACE_END
