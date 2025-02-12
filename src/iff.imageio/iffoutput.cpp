// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "iff_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace iff_pvt;


class IffOutput final : public ImageOutput {
public:
    IffOutput() { init(); }
    ~IffOutput() override
    {
        try {
            close();
        } catch (...) {
        }
    }
    const char* format_name(void) const override { return "iff"; }
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
    std::string m_filename;
    iff_pvt::IffFileHeader m_iff_header;
    std::vector<uint8_t> m_buf;
    unsigned int m_dither;
    std::vector<uint8_t> scratch;

    void init(void)
    {
        ioproxy_clear();
        m_filename.clear();
    }

    // writes information about iff file to give file
    bool write_header(iff_pvt::IffFileHeader& header);

    /// Helper: write buf[0..nitems-1], swap endianness if necessary
    template<typename T> bool write(const T* buf, size_t nitems = 1)
    {
        if (littleendian()
            && (std::is_same<T, uint16_t>::value
                || std::is_same<T, int16_t>::value
                || std::is_same<T, uint32_t>::value
                || std::is_same<T, int32_t>::value)) {
            T* newbuf = OIIO_ALLOCA(T, nitems);
            memcpy(newbuf, buf, nitems * sizeof(T));
            swap_endian(newbuf, nitems);
            buf = newbuf;
        }
        return iowrite(buf, sizeof(T), nitems);
    }

    bool write_short(uint16_t val) { return write(&val); }
    bool write_int(uint32_t val) { return write(&val); }

    bool write_str(string_view val, size_t round = 4)
    {
        bool ok = iowrite(val.data(), val.size());
        if (size_t extra = round_to_multiple(val.size(), round) - val.size()) {
            static const uint8_t pad[4] = { 0, 0, 0, 0 };
            ok &= iowrite(pad, extra);
        }
        return ok;
    }

    bool write_meta_string(string_view name, string_view val,
                           bool write_if_empty = false)
    {
        if (val.empty() && !write_if_empty)
            return true;
        return write_str(name) && write_int(uint32_t(val.size()))
               && (val.size() == 0 || write_str(val));
    }

    // helper to compress verbatim
    void compress_verbatim(const uint8_t*& in, uint8_t*& out, int size,
                           cspan<uint8_t> in_span, span<uint8_t> out_span);

    // helper to compress duplicate
    void compress_duplicate(const uint8_t*& in, uint8_t*& out, int size,
                            cspan<uint8_t> in_span, span<uint8_t> out_span);

    // helper to compress a rle channel
    size_t compress_rle_channel(const uint8_t* in, uint8_t* out, int size,
                                cspan<uint8_t> in_span, span<uint8_t> out_span);
};



// Obligatory material to make this a recognizable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput*
iff_output_imageio_create()
{
    return new IffOutput;
}

OIIO_EXPORT const char* iff_output_extensions[] = { "iff", "z", nullptr };

OIIO_PLUGIN_EXPORTS_END



int
IffOutput::supports(string_view feature) const
{
    return (feature == "tiles" || feature == "alpha" || feature == "nchannels"
            || feature == "ioproxy" || feature == "origin");
}



bool
IffOutput::open(const std::string& name, const ImageSpec& spec, OpenMode mode)
{
    // Autodesk Maya documentation:
    // "Maya Image File Format - IFF
    //
    // Maya supports images in the Interchange File Format (IFF).
    // IFF is a generic structured file access mechanism, and is not only
    // limited to images.
    //
    // The openimageio IFF implementation deals specifically with Maya IFF
    // images with its data blocks structured as follows:
    //
    // Header:
    // FOR4 <size> CIMG
    //  TBHD <size> flags, width, height, compression ...
    //    AUTH <size> attribute ...
    //    DATE <size> attribute ...
    //    FOR4 <size> TBMP
    // Tiles:
    //       RGBA <size> tile pixels
    //       RGBA <size> tile pixels
    //       RGBA <size> tile pixels
    //       ...

    // saving 'name' and 'spec' for later use
    m_filename = name;

    if (!check_open(mode, spec, { 0, 8192, 0, 8192, 0, 1, 0, 4 }))
        return false;
    // Maya docs say 8k is the limit

    // tiles always
    m_spec.tile_width  = tile_width();
    m_spec.tile_height = tile_height();
    m_spec.tile_depth  = 1;

    // This implementation only supports writing RGB and RGBA images as IFF
    if (m_spec.nchannels < 3 || m_spec.nchannels > 4) {
        errorfmt("Cannot write IFF file with {} channels", m_spec.nchannels);
        return false;
    }

    uint64_t xtiles = tile_width_size(m_spec.width);
    uint64_t ytiles = tile_height_size(m_spec.height);
    if (xtiles * ytiles >= (1 << 16)) {  // The format can't store it!
        errorfmt(
            "Too high a resolution ({}x{}), exceeds maximum of 64k tiles in the image\n",
            m_spec.width, m_spec.height);
        return false;
    }

    ioproxy_retrieve_from_config(m_spec);
    if (!ioproxy_use_or_open(name))
        return false;

    // IFF image files only supports UINT8 and UINT16.  If something
    // else was requested, revert to the one most likely to be readable
    // by any IFF reader: UINT8
    if (m_spec.format != TypeDesc::UINT8 && m_spec.format != TypeDesc::UINT16)
        m_spec.set_format(TypeDesc::UINT8);

    m_dither = (m_spec.format == TypeDesc::UINT8)
                   ? m_spec.get_int_attribute("oiio:dither", 0)
                   : 0;

    // check if the client wants the image to be run length encoded
    // currently only RGB RLE compression is supported, we default to RLE
    // as Maya does not handle non-compressed IFF's very well.
    m_iff_header.compression
        = (m_spec.get_string_attribute("compression") == "none") ? NONE : RLE;

    // we write the header of the file
    m_iff_header.x              = m_spec.x;
    m_iff_header.y              = m_spec.y;
    m_iff_header.width          = m_spec.width;
    m_iff_header.height         = m_spec.height;
    m_iff_header.tiles          = xtiles * ytiles;
    m_iff_header.pixel_bits     = m_spec.format == TypeDesc::UINT8 ? 8 : 16;
    m_iff_header.pixel_channels = m_spec.nchannels;
    m_iff_header.author         = m_spec.get_string_attribute("Artist");
    m_iff_header.date           = m_spec.get_string_attribute("DateTime");

    if (!write_header(m_iff_header)) {
        errorfmt("\"{}\": could not write iff header", m_filename);
        close();
        return false;
    }

    m_buf.resize(m_spec.image_bytes());

    return true;
}



bool
IffOutput::write_header(IffFileHeader& header)
{
    // write 'FOR4' type, with 0 length for now (to reserve it)
    if (!(write_str("FOR4") && write_int(0)))
        return false;

    // write 'CIMG' type
    if (!write_str("CIMG"))
        return false;

    // write 'TBHD' type
    if (!write_str("TBHD"))
        return false;

    // 'TBHD' length, 32 bytes
    if (!write_int(32))
        return false;

    if (!write_int(header.width) || !write_int(header.height))
        return false;

    // write prnum and prden (pixel aspect ratio? -- FIXME)
    if (!write_short(1) || !write_short(1))  //NOSONAR
        return false;

    // write flags and channels
    if (!write_int(header.pixel_channels == 3 ? RGB : RGBA)
        || !write_short(header.pixel_bits == 8 ? 0 : 1)
        || !write_short(header.tiles))
        return false;

    // write compression
    // 0 no compression
    // 1 RLE compression
    // 2 QRL (not supported)
    // 3 QR4 (not supported)
    if (!write_int(header.compression))
        return false;

    // write x and y
    if (!write_int(header.x) || !write_int(header.y))
        return false;

    // Write metadata
    write_meta_string("AUTH", header.author);
    write_meta_string("DATE", header.date);

    // for4 position for later user in close
    header.for4_start = iotell();

    // write 'FOR4' type, with 0 length to reserve it for now
    if (!write_str("FOR4") || !write_int(0))
        return false;

    // write 'TBMP' type
    if (!write_str("TBMP"))
        return false;

    return true;
}



bool
IffOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    if (!ioproxy_opened()) {
        errorfmt("write_scanline called but file is not open.");
        return false;
    }

    // scanline not used for Maya IFF, uses tiles instead.
    // Emulate by copying the scanline to the buffer we're accumulating.
    std::vector<unsigned char> scratch;
    data = to_native_scanline(format, data, xstride, scratch, m_dither, y, z);
    size_t scanlinesize = spec().scanline_bytes(true);
    size_t offset       = scanlinesize * (y - spec().y)
                    + scanlinesize * spec().height * (z - spec().z);
    memcpy(&m_buf[offset], data, scanlinesize);
    return false;
}



bool
IffOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    if (!ioproxy_opened()) {
        errorfmt("write_tile called but file is not open.");
        return false;
    }

    // auto stride
    m_spec.auto_stride(xstride, ystride, zstride, format, spec().nchannels,
                       spec().tile_width, spec().tile_height);

    // native tile
    data = to_native_tile(format, data, xstride, ystride, zstride, scratch,
                          m_dither, x, y, z);

    x -= m_spec.x;  // Account for offset, so x,y are file relative, not
    y -= m_spec.y;  // image relative

    // tile size
    int w  = m_spec.width;
    int tw = std::min(x + m_spec.tile_width, m_spec.width) - x;
    int th = std::min(y + m_spec.tile_height, m_spec.height) - y;

    // tile data
    int iy = 0;
    for (int oy = y; oy < y + th; oy++) {
        // in
        uint8_t* in_p = (uint8_t*)data
                        + (iy * m_spec.tile_width) * m_spec.pixel_bytes();
        // out
        uint8_t* out_p = &m_buf[0] + (oy * w + x) * m_spec.pixel_bytes();
        // copy
        memcpy(out_p, in_p, tw * m_spec.pixel_bytes());
        iy++;
    }

    return true;
}



inline bool
IffOutput::close(void)
{
    if (ioproxy_opened() && m_buf.size()) {
        // flip buffer to make write tile easier,
        // from tga.imageio:

        int bytespp = m_spec.pixel_bytes();

        std::vector<unsigned char> fliptmp(m_spec.width * bytespp);
        for (int y = 0; y < m_spec.height / 2; y++) {
            unsigned char* src
                = &m_buf[(m_spec.height - y - 1) * m_spec.width * bytespp];
            unsigned char* dst = &m_buf[y * m_spec.width * bytespp];

            memcpy(fliptmp.data(), src, m_spec.width * bytespp);
            memcpy(src, dst, m_spec.width * bytespp);
            memcpy(dst, fliptmp.data(), m_spec.width * bytespp);
        }

        // write y-tiles
        for (uint32_t ty = 0; ty < tile_height_size(m_spec.height); ty++) {
            // write x-tiles
            for (uint32_t tx = 0; tx < tile_width_size(m_spec.width); tx++) {
                // channels
                uint8_t channels = m_iff_header.pixel_channels;

                // set tile coordinates
                uint32_t xmin = tx * tile_width();
                uint32_t xmax
                    = std::min(xmin + tile_width(), uint32_t(m_spec.width)) - 1;
                uint32_t ymin = ty * tile_height();
                uint32_t ymax = std::min(ymin + tile_height(),
                                         uint32_t(m_spec.height))
                                - 1;

                // set width and height
                uint32_t tw = xmax - xmin + 1;
                uint32_t th = ymax - ymin + 1;

                // write 'RGBA' type
                if (!iowritefmt("RGBA"))
                    return false;

                // length.
                uint32_t length = tw * th * m_spec.pixel_bytes();

                // tile length.
                uint32_t tile_length = length;

                // align.
                length = align_size(length, 4);

                // append xmin, xmax, ymin and ymax.
                length += 8;

                // tile compression.
                bool tile_compress = (m_iff_header.compression == RLE);

                // set bytes.
                std::vector<uint8_t> scratch(tile_length);

                uint8_t* out_p = static_cast<uint8_t*>(&scratch[0]);

                // handle 8-bit data
                if (m_spec.format == TypeDesc::UINT8) {
                    if (tile_compress) {
                        uint32_t index = 0, size = 0;
                        std::vector<uint8_t> tmp;

                        // set bytes.
                        tmp.resize(tile_length * 2);

                        // map: RGB(A) to BGRA
                        for (int c = (channels * m_spec.channel_bytes()) - 1;
                             c >= 0; --c) {
                            std::vector<uint8_t> in(tw * th);
                            uint8_t* in_p = &in[0];

                            // set tile
                            for (uint32_t py = ymin; py <= ymax; py++) {
                                const uint8_t* in_dy
                                    = &m_buf[0] + py * m_spec.scanline_bytes();

                                for (uint32_t px = xmin; px <= xmax; px++) {
                                    // get pixel
                                    uint8_t pixel;
                                    const uint8_t* in_dx
                                        = in_dy + px * m_spec.pixel_bytes() + c;
                                    memcpy(&pixel, in_dx, 1);
                                    // set pixel
                                    *in_p++ = pixel;
                                }
                            }

                            // compress rle channel
                            size = compress_rle_channel(&in[0], &tmp[0] + index,
                                                        tw * th, in, tmp);
                            index += size;
                        }

                        // if size exceeds tile length write uncompressed

                        if (index < tile_length) {
                            memcpy(&scratch[0], &tmp[0], index);

                            // set tile length
                            tile_length = index;

                            // append xmin, xmax, ymin and ymax
                            length = index + 8;

                            // set length
                            uint32_t align = align_size(length, 4);
                            if (align > length) {
                                if (scratch.size() < index + align - length)
                                    scratch.resize(index + align - length);
                                out_p = &scratch[0] + index;
                                // Pad.
                                for (uint32_t i = 0; i < align - length; i++) {
                                    *out_p++ = '\0';
                                    tile_length++;
                                }
                            }
                        } else {
                            tile_compress = false;
                        }
                    }
                    if (!tile_compress) {
                        for (uint32_t py = ymin; py <= ymax; py++) {
                            const uint8_t* in_dy = &m_buf[0]
                                                   + (py * m_spec.width)
                                                         * m_spec.pixel_bytes();

                            for (uint32_t px = xmin; px <= xmax; px++) {
                                // Map: RGB(A)8 RGBA to BGRA
                                for (int c = channels - 1; c >= 0; --c) {
                                    // get pixel
                                    uint8_t pixel;
                                    const uint8_t* in_dx
                                        = in_dy + px * m_spec.pixel_bytes()
                                          + c * m_spec.channel_bytes();
                                    memcpy(&pixel, in_dx, 1);
                                    // set pixel
                                    *out_p++ = pixel;
                                }
                            }
                        }
                    }
                }
                // handle 16-bit data
                else if (m_spec.format == TypeDesc::UINT16) {
                    if (tile_compress) {
                        uint32_t index = 0, size = 0;
                        std::vector<uint8_t> tmp;

                        // set bytes.
                        tmp.resize(tile_length * 2);

                        // set map
                        std::vector<uint8_t> map;
                        if (littleendian()) {
                            int rgb16[]  = { 0, 2, 4, 1, 3, 5 };
                            int rgba16[] = { 0, 2, 4, 7, 1, 3, 5, 6 };
                            if (m_iff_header.pixel_channels == 3) {
                                map = std::vector<uint8_t>(rgb16, &rgb16[6]);
                            } else {
                                map = std::vector<uint8_t>(rgba16, &rgba16[8]);
                            }

                        } else {
                            int rgb16[]  = { 1, 3, 5, 0, 2, 4 };
                            int rgba16[] = { 1, 3, 5, 7, 0, 2, 4, 6 };
                            if (m_iff_header.pixel_channels == 3) {
                                map = std::vector<uint8_t>(rgb16, &rgb16[6]);
                            } else {
                                map = std::vector<uint8_t>(rgba16, &rgba16[8]);
                            }
                        }

                        // map: RRGGBB(AA) to BGR(A)BGR(A)
                        for (int c = (channels * m_spec.channel_bytes()) - 1;
                             c >= 0; --c) {
                            int mc = map[c];

                            std::vector<uint8_t> in(tw * th);
                            uint8_t* in_p = &in[0];

                            // set tile
                            for (uint32_t py = ymin; py <= ymax; py++) {
                                const uint8_t* in_dy
                                    = &m_buf[0] + py * m_spec.scanline_bytes();

                                for (uint32_t px = xmin; px <= xmax; px++) {
                                    // get pixel
                                    uint8_t pixel;
                                    const uint8_t* in_dx
                                        = in_dy + px * m_spec.pixel_bytes()
                                          + mc;
                                    memcpy(&pixel, in_dx, 1);
                                    // set pixel.
                                    *in_p++ = pixel;
                                }
                            }

                            // compress rle channel
                            size = compress_rle_channel(&in[0], &tmp[0] + index,
                                                        tw * th, in, tmp);
                            index += size;
                        }

                        // if size exceeds tile length write uncompressed

                        if (index < tile_length) {
                            memcpy(&scratch[0], &tmp[0], index);

                            // set tile length
                            tile_length = index;

                            // append xmin, xmax, ymin and ymax
                            length = index + 8;

                            // set length
                            uint32_t align = align_size(length, 4);
                            if (align > length) {
                                if (scratch.size() < index + align - length)
                                    scratch.resize(index + align - length);
                                out_p = &scratch[0] + index;
                                // Pad.
                                for (uint32_t i = 0; i < align - length; i++) {
                                    *out_p++ = '\0';
                                    tile_length++;
                                }
                            }
                        } else {
                            tile_compress = false;
                        }
                    }

                    if (!tile_compress) {
                        for (uint32_t py = ymin; py <= ymax; py++) {
                            const uint8_t* in_dy = &m_buf[0]
                                                   + (py * m_spec.width)
                                                         * m_spec.pixel_bytes();

                            for (uint32_t px = xmin; px <= xmax; px++) {
                                // map: RGB(A) to BGRA
                                for (int c = channels - 1; c >= 0; --c) {
                                    uint16_t pixel;
                                    const uint8_t* in_dx
                                        = in_dy + px * m_spec.pixel_bytes()
                                          + c * m_spec.channel_bytes();
                                    memcpy(&pixel, in_dx, 2);
                                    if (littleendian())
                                        swap_endian(&pixel);
                                    // set pixel
                                    *out_p++ = pixel & 0xff;
                                    *out_p++ = pixel >> 8;
                                }
                            }
                        }
                    }
                }

                // write 'RGBA' length
                if (!write(&length))
                    return false;

                // write xmin, xmax, ymin and ymax
                if (!write_short(xmin) || !write_short(ymin)
                    || !write_short(xmax) || !write_short(ymax))
                    return false;

                // write tile
                if (!iowrite(scratch.data(), tile_length))
                    return false;
            }
        }

        // set sizes
        uint32_t pos(iotell());

        uint32_t p0 = pos - 8;
        uint32_t p1 = p0 - m_iff_header.for4_start;

        // set pos
        ioseek(4);

        // write FOR4 <size> CIMG
        if (!write(&p0))
            return false;

        // set pos
        ioseek(m_iff_header.for4_start + 4);

        // write FOR4 <size> TBMP
        if (!write(&p1))
            return false;

        m_buf.resize(0);
        m_buf.shrink_to_fit();
    }

    init();
    return true;
}



void
IffOutput::compress_verbatim(const uint8_t*& in, uint8_t*& out, int size,
                             cspan<uint8_t> in_span, span<uint8_t> out_span)
{
    OIIO_DASSERT(in >= in_span.cbegin() && in + size <= in_span.cend());
    int count          = 1;
    unsigned char byte = 0;

    // two in a row or count
    for (; count < size; ++count) {
        if (in[count - 1] == in[count]) {
            if (byte == in[count - 1]) {
                count -= 2;
                break;
            }
        }
        byte = in[count - 1];
    }

    // copy
    OIIO_DASSERT(out >= out_span.begin() && out < out_span.end());
    *out++ = count - 1;
    span_memcpy(out, in, size_t(count), out_span, in_span);

    out += count;
    in += count;
}



void
IffOutput::compress_duplicate(const uint8_t*& in, uint8_t*& out, int size,
                              cspan<uint8_t> in_span, span<uint8_t> out_span)
{
    OIIO_DASSERT(in >= in_span.cbegin() && in + size <= in_span.cend());
    int count = 1;
    for (; count < size; ++count) {
        if (in[count - 1] != in[count])
            break;
    }

    const bool run   = count > 1;
    const int length = run ? 1 : count;

    // copy
    OIIO_DASSERT(out >= out_span.begin() && out + 2 <= out_span.end());
    *out++ = ((count - 1) & 0x7f) | (run << 7);
    *out   = *in;

    out += length;
    in += count;
}



size_t
IffOutput::compress_rle_channel(const uint8_t* in, uint8_t* out, int size,
                                cspan<uint8_t> in_span, span<uint8_t> out_span)
{
    const uint8_t* const _out = out;
    const uint8_t* const end  = in + size;
    OIIO_DASSERT(in >= in_span.cbegin() && in + size <= in_span.cend());
    OIIO_DASSERT(out >= out_span.begin() && out < out_span.end());

    while (in < end) {
        // find runs
        const int max = std::min(0x7f + 1, static_cast<int>(end - in));
        if (max > 0) {
            if (in < (end - 1) && in[0] == in[1]) {
                // compress duplicate
                compress_duplicate(in, out, max, in_span, out_span);
            } else {
                // compress verbatim
                compress_verbatim(in, out, max, in_span, out_span);
            }
        }
    }
    const size_t r = out - _out;
    return r;
}



OIIO_PLUGIN_NAMESPACE_END
