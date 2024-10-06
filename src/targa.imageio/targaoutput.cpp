// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <OpenImageIO/color.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/typedesc.h>

#include "targa_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace TGA_pvt;


class TGAOutput final : public ImageOutput {
public:
    TGAOutput();
    ~TGAOutput() override;
    const char* format_name(void) const override { return "targa"; }
    int supports(string_view feature) const override
    {
        return (feature == "alpha" || feature == "thumbnail"
                || feature == "thumbnail_after_write" || feature == "ioproxy");
    }
    bool open(const std::string& name, const ImageSpec& spec,
              OpenMode mode = Create) override;
    bool close() override;
    bool write_scanline(int y, int z, TypeDesc format, const void* data,
                        stride_t xstride) override;
    bool write_tile(int x, int y, int z, TypeDesc format, const void* data,
                    stride_t xstride, stride_t ystride,
                    stride_t zstride) override;
    bool set_thumbnail(const ImageBuf& thumb) override;

private:
    std::string m_filename;  ///< Stash the filename
    bool m_want_rle;         ///< Whether the client asked for RLE
    bool m_convert_alpha;    ///< Do we deassociate alpha?
    float m_gamma;           ///< Gamma to use for alpha conversion
    std::vector<unsigned char> m_scratch;
    int m_idlen;  ///< Length of the TGA ID block
    unsigned int m_dither;
    std::vector<unsigned char> m_tilebuffer;
    ImageBuf m_thumb;

    // Initialize private members to pre-opened state
    void init(void)
    {
        m_convert_alpha = true;
        m_gamma         = 1.0;
        m_thumb.reset();
        ioproxy_clear();
    }

    // Helper function to write the TGA 2.0 data fields, called by close()
    bool write_tga20_data_fields();

    /// Helper function to flush a non-run-length packet
    ///
    inline void flush_rawp(unsigned char*& src, int size, int start);

    /// Helper function to flush a run-length packet
    ///
    inline void flush_rlp(unsigned char* buf, int size);

    bool write(const void* buf, size_t itemsize, size_t nitems = 1)
    {
        return iowrite(buf, itemsize, nitems);
    }

    /// Helper -- write a 'short' with byte swapping if necessary
    bool write(uint16_t s)
    {
        if (bigendian())
            swap_endian(&s);
        return iowrite(&s, sizeof(s));
    }
    bool write(uint32_t i)
    {
        if (bigendian())
            swap_endian(&i);
        return iowrite(&i, sizeof(i));
    }
    bool write(uint8_t i) { return iowrite(&i, sizeof(i)); }

    // Helper -- pad with zeroes
    bool pad(size_t n = 1)
    {
        // up to 64 bytes at a time
        int zero[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        while (n > 0) {
            size_t bytes = std::min(n, size_t(64));
            if (!iowrite(zero, bytes))
                return false;
            n -= bytes;
        }
        return true;
    }

    // Helper -- write string, with padding and/or truncation
    bool write_padded(string_view s, size_t paddedlen)
    {
        size_t slen = std::min(s.length(), paddedlen - 1);
        return iowrite(s.data(), slen) && pad(paddedlen - slen);
    }

    template<class T>
    static void deassociateAlpha(T* data, int size, int channels,
                                 int alpha_channel, float gamma);
};



// Obligatory material to make this a recognizable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
targa_output_imageio_create()
{
    return new TGAOutput;
}

// OIIO_EXPORT int tga_imageio_version = OIIO_PLUGIN_VERSION;   // it's in tgainput.cpp

OIIO_EXPORT const char* targa_output_extensions[] = { "tga", "tpic", nullptr };

OIIO_PLUGIN_EXPORTS_END


TGAOutput::TGAOutput() { init(); }



TGAOutput::~TGAOutput()
{
    // Close, if not already done.
    close();
}



bool
TGAOutput::open(const std::string& name, const ImageSpec& userspec,
                OpenMode mode)
{
    if (!check_open(mode, userspec, { 0, 65535, 0, 65535, 0, 1, 0, 4 }))
        return false;

    // Offsets within the file are 32 bits. Guard against creating a TGA
    // file that (even counting the file footer or header) might exceed
    // this.
    if (m_spec.image_bytes() + sizeof(tga_header) + sizeof(tga_footer)
        >= (int64_t(1) << 32)) {
        errorfmt("Too large a TGA file");
        return false;
    }

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    // Force 8 bit integers
    m_spec.set_format(TypeDesc::UINT8);
    m_dither = m_spec.get_int_attribute("oiio:dither", 0);

    // check if the client wants the image to be run length encoded
    // currently only RGB RLE is supported
    m_want_rle = (m_spec.get_string_attribute("compression", "none")
                  != std::string("none"))
                 && m_spec.nchannels >= 3;

    // TGA does not dictate unassociated (un-"premultiplied") alpha but many
    // implementations assume it even if we set TGA_ALPHA_PREMULTIPLIED, so
    // always write unassociated alpha
    m_convert_alpha = m_spec.alpha_channel != -1
                      && !m_spec.get_int_attribute("oiio:UnassociatedAlpha", 0);

    m_gamma = m_spec.get_float_attribute("oiio:Gamma", 1.0);

    // prepare and write Targa header
    tga_header tga;
    memset(&tga, 0, sizeof(tga));
    tga.type   = m_spec.nchannels <= 2 ? TYPE_GRAY
                                       : (m_want_rle ? TYPE_RGB_RLE : TYPE_RGB);
    tga.bpp    = m_spec.nchannels * 8;
    tga.width  = m_spec.width;
    tga.height = m_spec.height;
#if 0  // no one seems to adhere to this part of the spec...
    tga.x_origin = m_spec.x;
    tga.y_origin = m_spec.y;
#endif

    // handle image ID; save it to disk later on
    std::string id = m_spec.get_string_attribute("targa:ImageID", "");
    // the format only allows for 255 bytes
    tga.idlen = std::min(id.length(), (size_t)255);
    m_idlen   = tga.idlen;

    if (m_spec.nchannels % 2 == 0)  // if we have alpha
        tga.attr = 8;               // 8 bits of alpha
    // force Y flip when using RLE
    // for raw (non-RLE) images we can use random access, so we can dump the
    // image in the default top-bottom scanline order for maximum
    // compatibility (not all software supports the Y flip flag); however,
    // once RLE kicks in, we lose the ability to predict the byte offsets of
    // scanlines, so we just dump the data in the order it comes in and use
    // this flag instead
    if (m_want_rle)
        tga.attr |= FLAG_Y_FLIP;
    // due to struct packing, we may get a corrupt header if we just dump the
    // struct to the file; to address that, write every member individually
    // save some typing. Note that these overloaded write calls will byte-swap
    // as needed.
    // Strutil::print("writing {} cmap_type {}\n", m_filename, int(tga.cmap_type));
    if (!write(tga.idlen) || !write(tga.cmap_type) || !write(tga.type)
        || !write(tga.cmap_first) || !write(tga.cmap_length)
        || !write(tga.cmap_size) || !write(tga.x_origin) || !write(tga.y_origin)
        || !write(tga.width) || !write(tga.height) || !write(tga.bpp)
        || !write(tga.attr)) {
        ioproxy_clear();
        return false;
    }

    // dump comment to file, don't bother about null termination
    if (tga.idlen) {
        if (!write(id.c_str(), tga.idlen)) {
            ioproxy_clear();
            return false;
        }
    }

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize(m_spec.image_bytes());

    return true;
}



bool
TGAOutput::write_tga20_data_fields()
{
    if (ioproxy_opened()) {
        // write out the TGA 2.0 data fields

        // FIXME: write out the developer area; according to Larry,
        // it's probably safe to ignore it altogether until someone complains
        // that it's missing :)

        ioseek(0, SEEK_END);

        // write out the thumbnail, if there is one
        uint32_t ofs_thumb = 0;
        if (m_thumb.initialized()) {
            unsigned char tw = m_thumb.spec().width;
            unsigned char th = m_thumb.spec().height;
            int tc           = m_thumb.spec().nchannels;
            OIIO_DASSERT(tw && th && tc == m_spec.nchannels);
            ofs_thumb = (uint32_t)iotell();
            // dump thumbnail size
            if (!write(tw) || !write(th)
                || !write(m_thumb.localpixels(), m_thumb.spec().image_bytes()))
                return false;
        }

        // prepare the footer
        tga_footer foot = { (uint32_t)iotell(), 0, "TRUEVISION-XFILE." };

        // write out the extension area

        // ext area size -- 2 bytes, always short(495)
        write(uint16_t(495));

        // author - 41 bytes
        write_padded(m_spec.get_string_attribute("Artist"), 41);

        // image comment - 324 bytes
        write_padded(m_spec.get_string_attribute("ImageDescription"), 324);

        // timestamp - 6 shorts (month, day, year, hour, minute, second)
        {
            string_view dt = m_spec.get_string_attribute("DateTime", "");
            uint16_t y = 0, m = 0, d = 0, h = 0, i = 0, s = 0;
            int ymd[3], hms[3];
            if (dt.length() > 0 && Strutil::parse_values(dt, "", ymd, ":")
                && Strutil::parse_values(dt, "", hms, ":")) {
                y = ymd[0];
                m = ymd[1];
                d = ymd[2];
                h = hms[0];
                i = hms[1];
                s = hms[2];
            }
            if (!write(m) || !write(d) || !write(y) || !write(h) || !write(i)
                || !write(s)) {
                return false;
            }
        }

        // job ID - 41 bytes
        write_padded(m_spec.get_string_attribute("DocumentName"), 41);

        // job time - 3 shorts (hours, minutes, seconds)
        {
            string_view jt = m_spec.get_string_attribute("targa:JobTime", "");
            uint16_t h = 0, m = 0, s = 0;
            int hms[3];
            if (jt.length() > 0 && Strutil::parse_values(jt, "", hms, ":")) {
                h = hms[0], m = hms[1], s = hms[2];
            }
            if (!write(h) || !write(m) || !write(s))
                return false;
        }

        // software ID -- 41 bytes
        write_padded(m_spec.get_string_attribute("Software"), 41);

        // software version - 3 bytes (first 2 bytes: version*100)
        if (!write(uint16_t(OIIO_VERSION)))
            return false;
        pad(1);

        // key colour (ARGB) -- punt and write 0
        pad(4);

        // pixel aspect ratio -- two shorts, giving a ratio
        {
            float ratio = m_spec.get_float_attribute("PixelAspectRatio", 1.f);
            float EPS   = 1E-5f;
            if (ratio >= (0.f + EPS)
                && ((ratio <= (1.f - EPS)) || (ratio >= (1.f + EPS)))) {
                // FIXME: invent a smarter way to convert to a vulgar fraction?
                // numerator
                write(uint16_t(ratio * 10000.f));  // numerator
                write(uint16_t(10000));            // denominator
            } else {
                // just dump two zeros in there
                write(uint16_t(0));
                write(uint16_t(0));
            }
        }

        // gamma -- two shorts, giving a ratio
        const ColorConfig& colorconfig = ColorConfig::default_colorconfig();
        string_view colorspace = m_spec.get_string_attribute("oiio:ColorSpace");
        if (colorconfig.equivalent(colorspace, "g22_rec709")) {
            m_gamma = 2.2f;
            write(uint16_t(m_gamma * 10.0f));
            write(uint16_t(10));
        } else if (colorconfig.equivalent(colorspace, "g18_rec709")) {
            m_gamma = 1.8f;
            write(uint16_t(m_gamma * 10.0f));
            write(uint16_t(10));
        } else if (Strutil::istarts_with(colorspace, "Gamma")) {
            // Extract gamma value from color space, if it's there
            Strutil::parse_word(colorspace);
            float g = Strutil::from_string<float>(colorspace);
            if (g >= 0.01f && g <= 10.0f /* sanity check */)
                m_gamma = g;
            // FIXME: invent a smarter way to convert to a vulgar fraction?
            // NOTE: the spec states that only 1 decimal place of precision
            // is needed, thus the expansion by 10
            // numerator
            write(uint16_t(std::round(m_gamma * 10.0f)));
            write(uint16_t(10));
        } else {
            // just dump two zeros in there
            write(uint16_t(0));
            write(uint16_t(0));
        }

        // offset to colour correction table - 4 bytes
        // FIXME: support this once it becomes clear how it's actually supposed
        // to be used... the spec is very unclear about this
        // for the time being just dump four NULL bytes
        pad(4);

        // offset to thumbnail - 4 bytes
        if (!write(ofs_thumb))
            return false;

        // offset to scanline table - 4 bytes
        // not used very widely, don't bother unless someone complains
        pad(4);

        // alpha type - one byte
        uint8_t at = (m_spec.nchannels % 2 == 0) ? TGA_ALPHA_USEFUL
                                                 : TGA_ALPHA_NONE;
        if (!write(at))
            return false;

        // write out the TGA footer
        if (!write(foot.ofs_ext) || !write(foot.ofs_dev)
            || !write(&foot.signature, 1, sizeof(foot.signature))) {
            return false;
        }
    }

    return true;
}



bool
TGAOutput::close()
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
        m_tilebuffer.shrink_to_fit();
    }

    ok &= write_tga20_data_fields();

    init();  // re-initialize
    return ok;
}



inline void
TGAOutput::flush_rlp(unsigned char* buf, int size)
{
    // early out
    if (size < 1)
        return;
    // write packet header
    uint8_t h = (size - 1) | 0x80;
    // write packet pixel
    if (!write(h) || !write(buf, m_spec.nchannels)) {
        // do something intelligent?
        return;
    }
}



inline void
TGAOutput::flush_rawp(unsigned char*& src, int size, int start)
{
    // early out
    if (size < 1)
        return;
    // write packet header
    uint8_t h = (size - 1) & ~0x80;
    if (!write(h))
        return;
    // rewind the scanline and flush packet pixels
    unsigned char buf[4];
    int n = m_spec.nchannels;
    for (int i = 0; i < size; i++) {
        if (n <= 2) {
            // 1- and 2-channels can write directly
            if (!write(src + start, n)) {
                return;
            }
        } else {
            // 3- and 4-channel must swap red and blue
            buf[0] = src[(start + i) * n + 2];
            buf[1] = src[(start + i) * n + 1];
            buf[2] = src[(start + i) * n + 0];
            if (n > 3)
                buf[3] = src[(start + i) * n + 3];
            if (!write(buf, n)) {
                return;
            }
        }
    }
}



template<class T>
void
TGAOutput::deassociateAlpha(T* data, int size, int channels, int alpha_channel,
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
                float alpha_deassociate
                    = OIIO::fast_pow_pos((float)max / data[alpha_channel],
                                         gamma);
                for (int c = 0; c < channels; c++)
                    if (c != alpha_channel)
                        data[c] = static_cast<T>(std::min(
                            max, (unsigned int)(data[c] * alpha_deassociate)));
            }
    }
}



bool
TGAOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    y -= m_spec.y;
    m_spec.auto_stride(xstride, format, spec().nchannels);
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);
    if (m_scratch.empty() || data != &m_scratch[0]) {
        m_scratch.assign((unsigned char*)data,
                         (unsigned char*)data + m_spec.scanline_bytes());
        data = &m_scratch[0];
    }

    if (m_convert_alpha) {
        deassociateAlpha((unsigned char*)data, m_spec.width, m_spec.nchannels,
                         m_spec.alpha_channel, m_gamma);
    }

    unsigned char* bdata = (unsigned char*)data;

    if (m_want_rle) {
        // Run Length Encoding
        // it's only profitable if n * b > 1 + b, where:
        // n is the number of pixels in a run
        // b is the pixel size in bytes
        // FIXME: optimize runs spanning across multiple scanlines?
        unsigned char buf[4]  = { 0, 0, 0, 0 };
        unsigned char buf2[4] = { 0, 0, 0, 0 };
        bool rlp              = false;
        int rlcount = 0, rawcount = 0;

        for (int x = 0; x < m_spec.width; x++) {
            // save off previous pixel
            memcpy(buf2, buf, sizeof(buf2));
            // read the new one
            switch (m_spec.nchannels) {
#if 0
            case 1:
                buf[0] = bdata[x ];
                break;
            case 2:
                buf[0] = bdata[(x * 2 + 0)];
                buf[1] = bdata[(x * 2 + 1)];
                break;
#endif
            case 3:
                buf[0] = bdata[(x * 3 + 2)];
                buf[1] = bdata[(x * 3 + 1)];
                buf[2] = bdata[(x * 3 + 0)];
                break;
            case 4:
                buf[0] = bdata[(x * 4 + 2)];
                buf[1] = bdata[(x * 4 + 1)];
                buf[2] = bdata[(x * 4 + 0)];
                buf[3] = bdata[(x * 4 + 3)];
                break;
            }

            //std::cerr << "[tga] x = " << x << "\n";

            if (x == 0) {  // initial encoder state
                rlp      = false;
                rlcount  = 0;
                rawcount = 1;
                continue;  // nothing to work with yet (need 2 pixels)
            }

            if (rlp) {  // in the middle of a run-length packet
                // flush the packet if the run ends or max packet size is hit
                if (rlcount < 0x80 && buf[0] == buf2[0] && buf[1] == buf2[1]
                    && buf[2] == buf2[2] && buf[3] == buf2[3])
                    rlcount++;
                else {
                    // run broken or max size hit, flush RL packet and start
                    // a new raw one
                    flush_rlp(&buf2[0], rlcount);
                    // count raw pixel
                    rawcount++;
                    // reset state
                    rlcount -= 0x80;
                    if (rlcount < 0)
                        rlcount = 0;
                    rlp = false;
                }
            } else {              // in the middle of a raw data packet
                if (rawcount > 0  // make sure we have material to check
                    && buf[0] == buf2[0] && buf[1] == buf2[1]
                    && buf[2] == buf2[2] && buf[3] == buf2[3]) {
                    // run continues, possibly material for RLE
                    if (rlcount == 0) {
                        // join the previous pixel into the run
                        rawcount--;
                        rlcount++;
                    }
                    rlcount++;
                } else {
                    // run broken
                    // apart from the pixel we've just read, add any remaining
                    // ones we may have considered for RLE
                    rawcount += 1 + rlcount;
                    rlcount = 0;
                    // flush the packet if max packet size is hit
                    if (rawcount >= 0x80) {
                        // subtract 128 instead of setting to 0 because there
                        // is a chance that rawcount is now > 128; if so, we'll
                        // catch the remainder in the next iteration
                        rawcount -= 0x80;
                        flush_rawp(bdata, 0x80, x - 0x80 + 1);
                    }
                }
                // check the encoding profitability condition
                //if (rlcount * m_spec.nchannels > 1 + m_spec.nchannels) {
                // NOTE: the condition below is valid, nchannels can be 1
                if (rlcount > 1 + 1 / m_spec.nchannels) {
                    // flush a packet of what we had so far
                    flush_rawp(bdata, rawcount, x - rawcount - rlcount + 1);
                    // reset state
                    rawcount = 0;
                    // mark this as a run-length packet
                    rlp = true;
                }
            }
        }
        // flush anything that may be left
        if (rlp)
            flush_rlp(&buf2[0], rlcount);
        else {
            rawcount += rlcount;
            flush_rawp(bdata, rawcount, m_spec.width - rawcount);
        }
    } else {
        // raw, non-compressed data
        // seek to the correct scanline
        int n     = m_spec.nchannels;
        int64_t w = m_spec.width;
        ioseek(18 + m_idlen + int64_t(m_spec.height - y - 1) * w * n);
        if (n <= 2) {
            // 1- and 2-channels can write directly
            if (!write(bdata, n, w)) {
                return false;
            }
        } else {
            // 3- and 4-channels must swap R and B
            std::vector<unsigned char> buf;
            buf.assign(bdata, bdata + n * w);
            for (int x = 0; x < m_spec.width; x++)
                std::swap(buf[x * n], buf[x * n + 2]);

            if (!write(&buf[0], n, w)) {
                return false;
            }
        }
    }

    return true;
}



bool
TGAOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_tilebuffer[0]);
}



bool
TGAOutput::set_thumbnail(const ImageBuf& thumb)
{
    if (!thumb.initialized() || thumb.spec().image_pixels() < 1
        || thumb.nchannels() != m_spec.nchannels) {
        // Zero size thumbnail or channels don't match
        return false;
    }
    // TARGA has a limitation of 256 res for thumbnail dimensions, and
    // must be UINT8.
    if (thumb.spec().width >= 256 || thumb.spec().height >= 256) {
        ROI roi(0, 256, 0, 256, 0, 1, 0, thumb.nchannels());
        float ratio = float(thumb.spec().width) / float(thumb.spec().height);
        if (ratio >= 1.0f) {
            roi.yend = (int)roundf(256.0f / ratio);
        } else {
            roi.xend = (int)roundf(256.0f * ratio);
        }
        m_thumb = ImageBufAlgo::resize(thumb, ImageBufAlgo::KWArgs(), roi,
                                       this->threads());
        if (thumb.pixeltype() != TypeUInt8)
            m_thumb = ImageBufAlgo::copy(m_thumb, TypeUInt8);
    } else {
        if (thumb.pixeltype() == TypeUInt8)
            m_thumb = thumb;
        else
            m_thumb = ImageBufAlgo::copy(thumb, TypeUInt8);
    }
    return true;
}


OIIO_PLUGIN_NAMESPACE_END
