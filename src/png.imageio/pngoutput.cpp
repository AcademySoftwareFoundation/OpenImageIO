// Copyright 2008-present Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>

#include "png_pvt.h"


OIIO_PLUGIN_NAMESPACE_BEGIN


class PNGOutput final : public ImageOutput {
public:
    PNGOutput();
    virtual ~PNGOutput();
    virtual const char* format_name(void) const override { return "png"; }
    virtual int supports(string_view feature) const override
    {
        return (feature == "alpha" || feature == "ioproxy");
    }
    virtual bool open(const std::string& name, const ImageSpec& spec,
                      OpenMode mode = Create) override;
    virtual bool close() override;
    virtual bool write_scanline(int y, int z, TypeDesc format, const void* data,
                                stride_t xstride) override;
    virtual bool write_tile(int x, int y, int z, TypeDesc format,
                            const void* data, stride_t xstride,
                            stride_t ystride, stride_t zstride) override;
    virtual bool set_ioproxy(Filesystem::IOProxy* ioproxy) override
    {
        m_io = ioproxy;
        return true;
    }

private:
    std::string m_filename;  ///< Stash the filename
    png_structp m_png;       ///< PNG read structure pointer
    png_infop m_info;        ///< PNG image info structure pointer
    unsigned int m_dither;
    int m_color_type;      ///< PNG color model type
    bool m_convert_alpha;  ///< Do we deassociate alpha?
    float m_gamma;         ///< Gamma to use for alpha conversion
    std::vector<unsigned char> m_scratch;
    std::vector<png_text> m_pngtext;
    std::vector<unsigned char> m_tilebuffer;
    std::unique_ptr<Filesystem::IOProxy> m_local_io;
    Filesystem::IOProxy* m_io = nullptr;
    bool m_err                = false;

    // Initialize private members to pre-opened state
    void init(void)
    {
        m_png           = NULL;
        m_info          = NULL;
        m_convert_alpha = true;
        m_gamma         = 1.0;
        m_pngtext.clear();
        m_local_io.reset();
        m_io  = nullptr;
        m_err = false;
    }

    // Add a parameter to the output
    bool put_parameter(const std::string& name, TypeDesc type,
                       const void* data);

    // Callback for PNG that writes via an IOProxy instead of writing
    // to a file.
    static void PngWriteCallback(png_structp png_ptr, png_bytep data,
                                 png_size_t length)
    {
        PNGOutput* pngoutput = (PNGOutput*)png_get_io_ptr(png_ptr);
        OIIO_DASSERT(pngoutput);
        size_t bytes = pngoutput->m_io->write(data, length);
        if (bytes != length) {
            pngoutput->errorf("Write error");
            pngoutput->m_err = true;
        }
    }

    static void PngFlushCallback(png_structp png_ptr)
    {
        PNGOutput* pngoutput = (PNGOutput*)png_get_io_ptr(png_ptr);
        OIIO_DASSERT(pngoutput);
        pngoutput->m_io->flush();
    }
};



// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
png_output_imageio_create()
{
    return new PNGOutput;
}

// OIIO_EXPORT int png_imageio_version = OIIO_PLUGIN_VERSION;   // it's in pnginput.cpp

OIIO_EXPORT const char* png_output_extensions[] = { "png", nullptr };

OIIO_PLUGIN_EXPORTS_END



PNGOutput::PNGOutput() { init(); }



PNGOutput::~PNGOutput()
{
    // Close, if not already done.
    close();
}



bool
PNGOutput::open(const std::string& name, const ImageSpec& userspec,
                OpenMode mode)
{
    if (mode != Create) {
        errorf("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    m_spec = userspec;  // Stash the spec

    // If not uint8 or uint16, default to uint8
    if (m_spec.format != TypeDesc::UINT8 && m_spec.format != TypeDesc::UINT16)
        m_spec.set_format(TypeDesc::UINT8);

    // See if we were requested to write to a memory buffer, and if so,
    // extract the pointer.
    auto ioparam = m_spec.find_attribute("oiio:ioproxy", TypeDesc::PTR);
    if (ioparam)
        m_io = ioparam->get<Filesystem::IOProxy*>();
    if (!m_io) {
        // If no proxy was supplied, create a file writer
        m_io = new Filesystem::IOFile(name, Filesystem::IOProxy::Mode::Write);
        m_local_io.reset(m_io);
    }
    if (!m_io || m_io->mode() != Filesystem::IOProxy::Mode::Write) {
        errorf("Could not open \"%s\"", name);
        return false;
    }

    std::string s = PNG_pvt::create_write_struct(m_png, m_info, m_color_type,
                                                 m_spec, this);
    if (s.length()) {
        close();
        errorf("%s", s);
        return false;
    }

    png_set_write_fn(m_png, this, PngWriteCallback, PngFlushCallback);

    png_set_compression_level(
        m_png, std::max(std::min(m_spec.get_int_attribute(
                                     "png:compressionLevel",
                                     6 /* medium speed vs size tradeoff */),
                                 Z_BEST_COMPRESSION),
                        Z_NO_COMPRESSION));
    std::string compression = m_spec.get_string_attribute("compression");
    if (compression.empty()) {
        png_set_compression_strategy(m_png, Z_DEFAULT_STRATEGY);
    } else if (Strutil::iequals(compression, "default")) {
        png_set_compression_strategy(m_png, Z_DEFAULT_STRATEGY);
    } else if (Strutil::iequals(compression, "filtered")) {
        png_set_compression_strategy(m_png, Z_FILTERED);
    } else if (Strutil::iequals(compression, "huffman")) {
        png_set_compression_strategy(m_png, Z_HUFFMAN_ONLY);
    } else if (Strutil::iequals(compression, "rle")) {
        png_set_compression_strategy(m_png, Z_RLE);
    } else if (Strutil::iequals(compression, "fixed")) {
        png_set_compression_strategy(m_png, Z_FIXED);
    } else {
        png_set_compression_strategy(m_png, Z_DEFAULT_STRATEGY);
    }

    png_set_filter(m_png, 0,
                   spec().get_int_attribute("png:filter", PNG_NO_FILTERS));
    // https://www.w3.org/TR/PNG-Encoders.html#E.Filter-selection
    // https://www.w3.org/TR/PNG-Rationale.html#R.Filtering
    // The official advice is to PNG_NO_FILTER for palette or < 8 bpp
    // images, but we and one of the others may be fine for >= 8 bit
    // greyscale or color images (they aren't very prescriptive, noting that
    // different flters may be better for different images.
    // We have found the tradeoff complex, in fact as seen in
    // https://github.com/OpenImageIO/oiio/issues/2645
    // where we showed that across several images, 8 (PNG_FILTER_NONE --
    // don't ask me how that's different from PNG_NO_FILTERS) had the
    // fastest performance, but also made the largest files. I had trouble
    // finding a filter choice that for "ordinary" images consistently
    // performed better than the default on both time and resulting file
    // size. So for now, we are keeping the default 0 (PNG_NO_FILTERS).

#if defined(PNG_SKIP_sRGB_CHECK_PROFILE) && defined(PNG_SET_OPTION_SUPPORTED)
    // libpng by default checks ICC profiles and are very strict, treating
    // it as a serious error if it doesn't match th profile it thinks is
    // right for sRGB. This call disables that behavior, which tends to have
    // many false positives. Some references to discussion about this:
    //    https://github.com/kornelski/pngquant/issues/190
    //    https://sourceforge.net/p/png-mng/mailman/message/32003609/
    //    https://bugzilla.gnome.org/show_bug.cgi?id=721135
    png_set_option(m_png, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);
#endif

    PNG_pvt::write_info(m_png, m_info, m_color_type, m_spec, m_pngtext,
                        m_convert_alpha, m_gamma);

    m_dither = (m_spec.format == TypeDesc::UINT8)
                   ? m_spec.get_int_attribute("oiio:dither", 0)
                   : 0;

    m_convert_alpha = m_spec.alpha_channel != -1
                      && !m_spec.get_int_attribute("oiio:UnassociatedAlpha", 0);

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize(m_spec.image_bytes());

    return true;
}



bool
PNGOutput::close()
{
    if (!m_io) {  // already closed
        init();
        return true;
    }

    bool ok = true;
    if (m_spec.tile_width) {
        // Handle tile emulation -- output the buffered pixels
        OIIO_ASSERT(m_tilebuffer.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap(m_tilebuffer);
    }

    if (m_png) {
        PNG_pvt::finish_image(m_png, m_info);
    }

    init();  // re-initialize
    return ok;
}



template<class T>
static void
deassociateAlpha(T* data, int size, int channels, int alpha_channel,
                 float gamma)
{
    unsigned int max = std::numeric_limits<T>::max();
    if (gamma == 1) {
        for (int x = 0; x < size; ++x, data += channels)
            if (data[alpha_channel])
                for (int c = 0; c < channels; c++)
                    if (c != alpha_channel) {
                        unsigned int f = data[c];
                        f              = (f * max) / data[alpha_channel];
                        data[c]        = (T)std::min(max, f);
                    }
    } else {
        for (int x = 0; x < size; ++x, data += channels)
            if (data[alpha_channel]) {
                // See associateAlpha() for an explanation.
                float alpha_deassociate = pow((float)max / data[alpha_channel],
                                              gamma);
                for (int c = 0; c < channels; c++)
                    if (c != alpha_channel)
                        data[c] = static_cast<T>(std::min(
                            max, (unsigned int)(data[c] * alpha_deassociate)));
            }
    }
}



bool
PNGOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    y -= m_spec.y;
    m_spec.auto_stride(xstride, format, spec().nchannels);
    const void* origdata = data;
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);
    if (data == origdata) {
        m_scratch.assign((unsigned char*)data,
                         (unsigned char*)data + m_spec.scanline_bytes());
        data = &m_scratch[0];
    }

    // PNG specifically dictates unassociated (un-"premultiplied") alpha
    if (m_convert_alpha) {
        if (m_spec.format == TypeDesc::UINT16)
            deassociateAlpha((unsigned short*)data, m_spec.width,
                             m_spec.nchannels, m_spec.alpha_channel, m_gamma);
        else
            deassociateAlpha((unsigned char*)data, m_spec.width,
                             m_spec.nchannels, m_spec.alpha_channel, m_gamma);
    }

    // PNG is always big endian
    if (littleendian() && m_spec.format == TypeDesc::UINT16)
        swap_endian((unsigned short*)data, m_spec.width * m_spec.nchannels);

    if (!PNG_pvt::write_row(m_png, (png_byte*)data)) {
        errorf("PNG library error");
        return false;
    }

    return true;
}



bool
PNGOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_tilebuffer[0]);
}


OIIO_PLUGIN_NAMESPACE_END
