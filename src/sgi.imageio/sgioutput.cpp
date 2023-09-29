// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "sgi_pvt.h"


OIIO_PLUGIN_NAMESPACE_BEGIN

class SgiOutput final : public ImageOutput {
public:
    SgiOutput() {}
    ~SgiOutput() override { close(); }
    const char* format_name(void) const override { return "sgi"; }
    int supports(string_view feature) const override;
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool close(void) override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;

private:
    std::string m_filename;
    std::vector<unsigned char> m_scratch;
    unsigned int m_dither;
    std::vector<unsigned char> m_tilebuffer;

    void init() { ioproxy_clear(); }

    bool create_and_write_header();

    /// Helper - write, with error detection
    template<class T>
    bool fwrite(const T* buf, size_t itemsize = sizeof(T), size_t nitems = 1)
    {
        return iowrite(buf, itemsize, nitems);
    }
};



// Obligatory material to make this a recognizable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN
OIIO_EXPORT ImageOutput*
sgi_output_imageio_create()
{
    return new SgiOutput;
}
OIIO_EXPORT const char* sgi_output_extensions[] = { "sgi", "rgb",  "rgba", "bw",
                                                    "int", "inta", nullptr };
OIIO_PLUGIN_EXPORTS_END



int
SgiOutput::supports(string_view feature) const
{
    return (feature == "alpha" || feature == "nchannels"
            || feature == "ioproxy");
}



bool
SgiOutput::open(const std::string& name, const ImageSpec& spec, OpenMode mode)
{
    if (!check_open(mode, spec, { 0, 65535, 0, 65535, 0, 1, 0, 256 }))
        return false;

    m_filename = name;

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    // SGI image files only supports UINT8 and UINT16.  If something
    // else was requested, revert to the one most likely to be readable
    // by any SGI reader: UINT8
    if (m_spec.format != TypeDesc::UINT8 && m_spec.format != TypeDesc::UINT16)
        m_spec.set_format(TypeDesc::UINT8);
    m_dither = (m_spec.format == TypeDesc::UINT8)
                   ? m_spec.get_int_attribute("oiio:dither", 0)
                   : 0;

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize(m_spec.image_bytes());

    return create_and_write_header();
}



bool
SgiOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    y    = m_spec.height - y - 1;
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);

    // In SGI format all channels are saved to file separately: first, all
    // channel 1 scanlines are saved, then all channel2 scanlines are saved
    // and so on.
    //
    // Note that since SGI images are pretty archaic and most probably
    // people won't be too picky about full flexibility writing them, we
    // content ourselves with only writing uncompressed data, and don't
    // attempt to write with RLE encoding.

    size_t bpc = m_spec.format.size();  // bytes per channel
    std::unique_ptr<unsigned char[]> channeldata(
        new unsigned char[m_spec.width * bpc]);

    for (int64_t c = 0; c < m_spec.nchannels; ++c) {
        unsigned char* cdata = (unsigned char*)data + c * bpc;
        for (int64_t x = 0; x < m_spec.width; ++x) {
            channeldata[x * bpc] = cdata[0];
            if (bpc == 2)
                channeldata[x * bpc + 1] = cdata[1];
            cdata += m_spec.nchannels * bpc;  // advance to next pixel
        }
        if (bpc == 2 && littleendian())
            swap_endian((unsigned short*)&channeldata[0], m_spec.width);
        ptrdiff_t scanline_offset = sgi_pvt::SGI_HEADER_LEN
                                    + ptrdiff_t(c * m_spec.height + y)
                                          * m_spec.width * bpc;
        ioseek(scanline_offset);
        if (!iowrite(&channeldata[0], 1, m_spec.width * bpc)) {
            return false;
        }
    }

    return true;
}



bool
SgiOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_tilebuffer[0]);
}



bool
SgiOutput::close()
{
    if (!ioproxy_opened()) {  // already closed
        init();
        return true;
    }

    bool ok = true;
    if (m_spec.tile_width) {
        // Handle tile emulation -- output the buffered pixels
        OIIO_ASSERT(m_tilebuffer.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_tilebuffer[0]);
        m_tilebuffer.clear();
        m_tilebuffer.shrink_to_fit();
    }

    init();
    return ok;
}



bool
SgiOutput::create_and_write_header()
{
    sgi_pvt::SgiHeader sgi_header;
    sgi_header.magic   = sgi_pvt::SGI_MAGIC;
    sgi_header.storage = sgi_pvt::VERBATIM;
    sgi_header.bpc     = m_spec.format.size();

    if (m_spec.height == 1 && m_spec.nchannels == 1)
        sgi_header.dimension = sgi_pvt::ONE_SCANLINE_ONE_CHANNEL;
    else if (m_spec.nchannels == 1)
        sgi_header.dimension = sgi_pvt::MULTI_SCANLINE_ONE_CHANNEL;
    else
        sgi_header.dimension = sgi_pvt::MULTI_SCANLINE_MULTI_CHANNEL;

    sgi_header.xsize  = m_spec.width;
    sgi_header.ysize  = m_spec.height;
    sgi_header.zsize  = m_spec.nchannels;
    sgi_header.pixmin = 0;
    sgi_header.pixmax = (sgi_header.bpc == 1) ? 255 : 65535;
    sgi_header.dummy  = 0;

    auto imagename = m_spec.get_string_attribute("ImageDescription");
    Strutil::safe_strcpy(sgi_header.imagename, imagename, 80);

    sgi_header.colormap = sgi_pvt::NORMAL;

    if (littleendian()) {
        swap_endian(&sgi_header.magic);
        swap_endian(&sgi_header.dimension);
        swap_endian(&sgi_header.xsize);
        swap_endian(&sgi_header.ysize);
        swap_endian(&sgi_header.zsize);
        swap_endian(&sgi_header.pixmin);
        swap_endian(&sgi_header.pixmax);
        swap_endian(&sgi_header.colormap);
    }

    char dummy[404] = { 0 };
    if (!fwrite(&sgi_header.magic) || !fwrite(&sgi_header.storage)
        || !fwrite(&sgi_header.bpc) || !fwrite(&sgi_header.dimension)
        || !fwrite(&sgi_header.xsize) || !fwrite(&sgi_header.ysize)
        || !fwrite(&sgi_header.zsize) || !fwrite(&sgi_header.pixmin)
        || !fwrite(&sgi_header.pixmax) || !fwrite(&sgi_header.dummy)
        || !fwrite(sgi_header.imagename, 1, 80) || !fwrite(&sgi_header.colormap)
        || !fwrite(dummy, 404, 1)) {
        errorfmt("Error writing to \"{}\"", m_filename);
        return false;
    }
    return true;
}

OIIO_PLUGIN_NAMESPACE_END
