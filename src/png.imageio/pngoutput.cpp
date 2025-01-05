// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

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
    ~PNGOutput() override;
    const char* format_name(void) const override { return "png"; }
    int supports(string_view feature) const override
    {
        return (feature == "alpha" || feature == "ioproxy"
#ifdef PNG_eXIf_SUPPORTED
                || feature == "exif"
#endif
        );
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool close() override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                         const void* data, stride_t xstride = AutoStride,
                         stride_t ystride = AutoStride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;

private:
    std::string m_filename;  ///< Stash the filename
    png_structp m_png;       ///< PNG read structure pointer
    png_infop m_info;        ///< PNG image info structure pointer
    unsigned int m_dither;
    int m_color_type;       ///< PNG color model type
    bool m_convert_alpha;   ///< Do we deassociate alpha?
    bool m_need_swap;       ///< Do we need to swap bytes?
    bool m_linear_premult;  ///< Do premult for sRGB images in linear
    bool m_srgb   = false;  ///< It's an sRGB image (not gamma)
    float m_gamma = 1.0f;   ///< Gamma to use for alpha conversion
    std::vector<unsigned char> m_scratch;
    std::vector<png_text> m_pngtext;
    std::vector<unsigned char> m_tilebuffer;
    bool m_err = false;

    // Initialize private members to pre-opened state
    void init(void)
    {
        m_png            = NULL;
        m_info           = NULL;
        m_convert_alpha  = true;
        m_need_swap      = false;
        m_linear_premult = false;
        m_srgb           = false;
        m_err            = false;
        m_gamma          = 1.0;
        m_pngtext.clear();
        ioproxy_clear();
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
        if (!pngoutput->iowrite(data, length))
            pngoutput->m_err = true;
    }

    static void PngFlushCallback(png_structp png_ptr)
    {
        PNGOutput* pngoutput = (PNGOutput*)png_get_io_ptr(png_ptr);
        OIIO_DASSERT(pngoutput);
        pngoutput->ioproxy()->flush();
    }

    template<class T>
    void deassociateAlpha(T* data, size_t npixels, int channels,
                          int alpha_channel, bool srgb, float gamma);
};



// Obligatory material to make this a recognizable imageio plugin:
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
    try {
        close();
    } catch (...) {
    }
}



bool
PNGOutput::open(const std::string& name, const ImageSpec& userspec,
                OpenMode mode)
{
    if (!check_open(mode, userspec, { 0, 65535, 0, 65535, 0, 1, 0, 256 }))
        return false;

    // If not uint8 or uint16, default to uint8
    if (m_spec.format != TypeDesc::UINT8 && m_spec.format != TypeDesc::UINT16)
        m_spec.set_format(TypeDesc::UINT8);

    // See if we were requested to write to a memory buffer, and if so,
    // extract the pointer.
    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    std::string s = PNG_pvt::create_write_struct(m_png, m_info, m_color_type,
                                                 m_spec, this);
    if (s.length()) {
        close();
        errorfmt("{}", s);
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
    } else if (Strutil::iequals(compression, "pngfast")) {
        png_set_compression_strategy(m_png, Z_DEFAULT_STRATEGY);
        png_set_compression_level(m_png, Z_BEST_SPEED);
    } else if (Strutil::iequals(compression, "none")) {
        png_set_compression_strategy(m_png, Z_NO_COMPRESSION);
        png_set_compression_level(m_png, 0);
    } else {
        png_set_compression_strategy(m_png, Z_DEFAULT_STRATEGY);
    }

    m_need_swap = (m_spec.format == TypeDesc::UINT16 && littleendian());

    m_linear_premult = m_spec.get_int_attribute("png:linear_premult",
                                                OIIO::get_int_attribute(
                                                    "png:linear_premult"));

    png_set_filter(m_png, 0,
                   spec().get_int_attribute("png:filter", PNG_NO_FILTERS));
    // https://www.w3.org/TR/PNG-Encoders.html#E.Filter-selection
    // https://www.w3.org/TR/PNG-Rationale.html#R.Filtering
    // The official advice is to PNG_NO_FILTER for palette or < 8 bpp
    // images, but we and one of the others may be fine for >= 8 bit
    // greyscale or color images (they aren't very prescriptive, noting that
    // different filters may be better for different images.
    // We have found the tradeoff complex, in fact as seen in
    // https://github.com/AcademySoftwareFoundation/OpenImageIO/issues/2645
    // where we showed that across several images, 8 (PNG_FILTER_NONE --
    // don't ask me how that's different from PNG_NO_FILTERS) had the
    // fastest performance, but also made the largest files. I had trouble
    // finding a filter choice that for "ordinary" images consistently
    // performed better than the default on both time and resulting file
    // size. So for now, we are keeping the default 0 (PNG_NO_FILTERS).

#if defined(PNG_SKIP_sRGB_CHECK_PROFILE) && defined(PNG_SET_OPTION_SUPPORTED)
    // libpng by default checks ICC profiles and are very strict, treating
    // it as a serious error if it doesn't match the profile it thinks is
    // right for sRGB. This call disables that behavior, which tends to have
    // many false positives. Some references to discussion about this:
    //    https://github.com/kornelski/pngquant/issues/190
    //    https://sourceforge.net/p/png-mng/mailman/message/32003609/
    //    https://bugzilla.gnome.org/show_bug.cgi?id=721135
    png_set_option(m_png, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);
#endif

    s = PNG_pvt::write_info(m_png, m_info, m_color_type, m_spec, m_pngtext,
                            m_convert_alpha, m_srgb, m_gamma);

    if (s.length()) {
        close();
        errorfmt("{}", s);
        return false;
    }

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
        std::vector<unsigned char>().swap(m_tilebuffer);
    }

    if (m_png) {
        PNG_pvt::write_end(m_png, m_info);
        if (m_png || m_info)
            PNG_pvt::destroy_write_struct(m_png, m_info);
        m_png  = nullptr;
        m_info = nullptr;
    }

    init();  // re-initialize
    return ok;
}



template<class T>
void
PNGOutput::deassociateAlpha(T* data, size_t npixels, int channels,
                            int alpha_channel, bool srgb, float gamma)
{
    if (srgb && m_linear_premult) {
        // sRGB with request to do unpremult in linear space
        for (size_t x = 0; x < npixels; ++x, data += channels) {
            DataArrayProxy<T, float> val(data);
            float alpha = val[alpha_channel];
            if (alpha != 0.0f && alpha != 1.0f) {
                for (int c = 0; c < channels; c++) {
                    if (c != alpha_channel) {
                        float f = sRGB_to_linear(val[c]);
                        val[c]  = linear_to_sRGB(f / alpha);
                    }
                }
            }
        }
    } else if (gamma != 1.0f && m_linear_premult) {
        // Gamma correction with request to do unpremult in linear space
        for (size_t x = 0; x < npixels; ++x, data += channels) {
            DataArrayProxy<T, float> val(data);
            float alpha = val[alpha_channel];
            if (alpha != 0.0f && alpha != 1.0f) {
                // See associateAlpha() for an explanation.
                float alpha_deassociate = pow(1.0f / val[alpha_channel], gamma);
                for (int c = 0; c < channels; c++) {
                    if (c != alpha_channel)
                        val[c] = val[c] * alpha_deassociate;
                }
            }
        }
    } else {
        // Do the unpremult directly on the values. This is correct for the
        // "gamma=1" case, and is also commonly what is needed for many sRGB
        // images (even though it's technically wrong in that case).
        for (size_t x = 0; x < npixels; ++x, data += channels) {
            DataArrayProxy<T, float> val(data);
            float alpha = val[alpha_channel];
            if (alpha != 0.0f && alpha != 1.0f) {
                for (int c = 0; c < channels; c++) {
                    if (c != alpha_channel)
                        val[c] = data[c] / alpha;
                }
            }
        }
    }
}



bool
PNGOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    m_spec.auto_stride(xstride, format, spec().nchannels);
    const void* origdata = data;
    if (format == TypeUnknown)
        format = m_spec.format;

    // PNG specifically dictates unassociated (un-"premultiplied") alpha.
    // If we need to unassociate alpha, do it in float.
    std::unique_ptr<float[]> unassoc_scratch;
    if (m_convert_alpha) {
        size_t nvals     = size_t(m_spec.width) * size_t(m_spec.nchannels);
        float* floatvals = nullptr;
        if (nvals * sizeof(float) <= (1 << 16)) {
            floatvals = OIIO_ALLOCA(float, nvals);  // small enough for stack
        } else {
            unassoc_scratch.reset(new float[nvals]);
            floatvals = unassoc_scratch.get();
        }
        // Contiguize and convert to float
        OIIO::convert_image(m_spec.nchannels, m_spec.width, 1, 1, data, format,
                            xstride, AutoStride, AutoStride, floatvals,
                            TypeFloat, AutoStride, AutoStride, AutoStride);
        // Deassociate alpha
        deassociateAlpha(floatvals, size_t(m_spec.width), m_spec.nchannels,
                         m_spec.alpha_channel, m_srgb, m_gamma);
        data    = floatvals;
        format  = TypeFloat;
        xstride = size_t(m_spec.nchannels) * sizeof(float);
    }

    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);
    if (data == origdata && (m_convert_alpha || m_need_swap)) {
        m_scratch.assign((unsigned char*)data,
                         (unsigned char*)data + m_spec.scanline_bytes());
        data = &m_scratch[0];
    }

    // PNG is always big endian
    if (m_need_swap)
        swap_endian((unsigned short*)data, m_spec.width * m_spec.nchannels);

    if (!PNG_pvt::write_row(m_png, (png_byte*)data)) {
        errorfmt("PNG library error");
        return false;
    }

    return true;
}



bool
PNGOutput::write_scanlines(int ybegin, int yend, int z, TypeDesc format,
                           const void* data, stride_t xstride, stride_t ystride)
{
#if 0
    // For testing/benchmarking: just implement write_scanlines in terms of
    // individual calls to write_scanline.
    for (int y = ybegin ; y < yend; ++y) {
        if (!write_scanline(y, z, format, data, xstride))
            return false;
        data = (const char*)data + ystride;
    }
    return true;
#else
    stride_t zstride = AutoStride;
    m_spec.auto_stride(xstride, ystride, zstride, format, m_spec.nchannels,
                       m_spec.width, m_spec.height);
    const void* origdata = data;
    if (format == TypeUnknown)
        format = m_spec.format;

    // PNG specifically dictates unassociated (un-"premultiplied") alpha.
    // If we need to unassociate alpha, do it in float.
    std::unique_ptr<float[]> unassoc_scratch;
    size_t npixels = size_t(m_spec.width) * size_t(yend - ybegin);
    size_t nvals   = npixels * size_t(m_spec.nchannels);
    if (m_convert_alpha) {
        unassoc_scratch.reset(new float[nvals]);
        float* floatvals = unassoc_scratch.get();
        // Contiguize and convert to float
        OIIO::convert_image(m_spec.nchannels, m_spec.width, yend - ybegin, 1,
                            data, format, xstride, ystride, AutoStride,
                            floatvals, TypeFloat, AutoStride, AutoStride,
                            AutoStride);
        // Deassociate alpha
        deassociateAlpha(floatvals, npixels, m_spec.nchannels,
                         m_spec.alpha_channel, m_srgb, m_gamma);
        data    = floatvals;
        format  = TypeFloat;
        xstride = size_t(m_spec.nchannels) * sizeof(float);
        ystride = xstride * size_t(m_spec.width);
        zstride = ystride * size_t(m_spec.height);
    }

    data = to_native_rectangle(m_spec.x, m_spec.x + m_spec.width, ybegin, yend,
                               z, z + 1, format, data, xstride, ystride,
                               zstride, m_scratch, m_dither, 0, ybegin, z);
    if (data == origdata && (m_convert_alpha || m_need_swap)) {
        m_scratch.assign((unsigned char*)data,
                         (unsigned char*)data + nvals * m_spec.format.size());
        data = m_scratch.data();
    }

    // PNG is always big endian
    if (m_need_swap)
        swap_endian((unsigned short*)data, nvals);

    if (!PNG_pvt::write_rows(m_png, (png_byte*)data, yend - ybegin,
                             stride_t(m_spec.width) * m_spec.nchannels
                                 * m_spec.format.size())) {
        errorfmt("PNG library error");
        return false;
    }
#endif

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
