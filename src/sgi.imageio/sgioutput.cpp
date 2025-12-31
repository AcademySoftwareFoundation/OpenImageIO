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
    bool m_want_rle;
    std::vector<unsigned char> m_uncompressed_image;

    void init() { ioproxy_clear(); }

    bool create_and_write_header();

    bool write_scanline_raw(int y, const unsigned char* data);
    bool write_scanline_rle(int y, const unsigned char* data, int64_t& offset,
                            std::vector<int>& start_table,
                            std::vector<int>& length_table);
    bool write_buffered_pixels();

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

    m_want_rle = m_spec.get_string_attribute("compression") == "rle";

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image. RLE is treated similarly.
    if (m_want_rle || (m_spec.tile_width && m_spec.tile_height))
        m_uncompressed_image.resize(m_spec.image_bytes());

    return create_and_write_header();
}



bool
SgiOutput::write_scanline(int y, int z, TypeDesc format, const void* data,
                          stride_t xstride)
{
    y    = m_spec.height - y - 1;
    data = to_native_scanline(format, data, xstride, m_scratch, m_dither, y, z);

    // If we are writing RLE data, just copy into the uncompressed buffer
    if (m_want_rle) {
        const auto scaneline_size = m_spec.scanline_bytes();
        memcpy(&m_uncompressed_image[y * scaneline_size], data, scaneline_size);

        return true;
    }

    return write_scanline_raw(y, (const unsigned char*)data);
}



bool
SgiOutput::write_tile(int x, int y, int z, TypeDesc format, const void* data,
                      stride_t xstride, stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer(x, y, z, format, data, xstride, ystride,
                                     zstride, &m_uncompressed_image[0]);
}



bool
SgiOutput::write_scanline_raw(int y, const unsigned char* data)
{
    // In SGI format all channels are saved to file separately: first, all
    // channel 1 scanlines are saved, then all channel2 scanlines are saved
    // and so on.

    size_t bpc = m_spec.format.size();  // bytes per channel
    std::unique_ptr<unsigned char[]> channeldata(
        new unsigned char[m_spec.width * bpc]);

    for (int64_t c = 0; c < m_spec.nchannels; ++c) {
        const unsigned char* cdata = data + c * bpc;
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



static bool
data_equals(const unsigned char* data, int bpc, imagesize_t off1,
            imagesize_t off2)
{
    if (bpc == 1) {
        return data[off1] == data[off2];
    } else {
        return data[off1] == data[off2] && data[off1 + 1] == data[off2 + 1];
    }
}



static void
data_set(unsigned char* data, int bpc, imagesize_t off,
         const unsigned char* val)
{
    if (bpc == 1) {
        data[off] = val[0];
    } else {
        data[off]     = val[1];
        data[off + 1] = val[0];
    }
}



static void
data_set(unsigned char* data, int bpc, imagesize_t off, const short val)
{
    if (bpc == 1) {
        data[off] = static_cast<unsigned char>(val);
    } else {
        data[off]     = static_cast<unsigned char>(val >> 8);
        data[off + 1] = static_cast<unsigned char>(val & 0xFF);
    }
}



bool
SgiOutput::write_scanline_rle(int y, const unsigned char* data, int64_t& offset,
                              std::vector<int>& offset_table,
                              std::vector<int>& length_table)
{
    const size_t bpc     = m_spec.format.size();  // bytes per channel
    const size_t xstride = m_spec.nchannels * bpc;
    const imagesize_t scanline_bytes = m_spec.scanline_bytes();

    // Account for the worst case length when every pixel is different
    m_scratch.resize(bpc * (m_spec.width + (m_spec.width / 127 + 2)));

    for (int64_t c = 0; c < m_spec.nchannels; ++c) {
        const unsigned char* cdata = data + c * bpc;

        imagesize_t out = 0;
        imagesize_t pos = 0;
        while (pos < scanline_bytes) {
            imagesize_t start = pos;
            // Find the first run meeting a minimum length of 3
            imagesize_t ahead_1 = pos + xstride;
            imagesize_t ahead_2 = pos + xstride * 2;
            while (ahead_2 < scanline_bytes
                   && (!data_equals(cdata, bpc, ahead_1, ahead_2)
                       || !data_equals(cdata, bpc, pos, ahead_1))) {
                pos += xstride;
                ahead_1 += xstride;
                ahead_2 += xstride;
            }
            if (ahead_2 >= scanline_bytes) {
                // No more runs, just dump the rest as literals
                pos = scanline_bytes;
            }
            int count = int((pos - start) / xstride);
            while (count) {
                int todo = (count > 127) ? 127 : count;
                count -= todo;
                data_set(m_scratch.data(), bpc, out, 0x80 | todo);
                out += bpc;
                while (todo) {
                    data_set(m_scratch.data(), bpc, out, cdata + start);
                    out += bpc;
                    start += xstride;
                    todo -= 1;
                }
            }
            start = pos;
            if (start >= scanline_bytes)
                break;
            pos += xstride;
            while (pos < scanline_bytes
                   && data_equals(cdata, bpc, start, pos)) {
                pos += xstride;
            }
            count = int((pos - start) / xstride);
            while (count) {
                int curr_run = (count > 127) ? 127 : count;
                count -= curr_run;
                data_set(m_scratch.data(), bpc, out, curr_run);
                out += bpc;
                data_set(m_scratch.data(), bpc, out, cdata + start);
                out += bpc;
            }
        }
        data_set(m_scratch.data(), bpc, out, short(0));
        out += bpc;

        // Fill in details about the scanline
        const int table_index     = c * m_spec.height + y;
        offset_table[table_index] = static_cast<int>(offset);
        length_table[table_index] = static_cast<int>(out);

        // Write the compressed data
        if (!iowrite(&m_scratch[0], 1, out))
            return false;
        offset += out;
    }

    return true;
}



bool
SgiOutput::write_buffered_pixels()
{
    OIIO_ASSERT(m_uncompressed_image.size());

    const auto scanline_bytes = m_spec.scanline_bytes();
    if (m_want_rle) {
        // Prepare RLE tables
        const int64_t table_size       = m_spec.height * m_spec.nchannels;
        const int64_t table_size_bytes = table_size * sizeof(int);
        std::vector<int> offset_table;
        std::vector<int> length_table;
        offset_table.resize(table_size);
        length_table.resize(table_size);

        // Skip over the tables and start at the data area
        int64_t offset = sgi_pvt::SGI_HEADER_LEN + 2 * table_size_bytes;
        ioseek(offset);

        // Write RLE compressed data
        for (int y = 0; y < m_spec.height; ++y) {
            const unsigned char* scanline_data
                = &m_uncompressed_image[y * scanline_bytes];
            if (!write_scanline_rle(y, scanline_data, offset, offset_table,
                                    length_table))
                return false;
        }

        // Write the tables now that they're filled in with offsets/lengths
        ioseek(sgi_pvt::SGI_HEADER_LEN);
        if (littleendian()) {
            swap_endian(&offset_table[0], table_size);
            swap_endian(&length_table[0], table_size);
        }
        if (!iowrite(&offset_table[0], 1, table_size_bytes))
            return false;
        if (!iowrite(&length_table[0], 1, table_size_bytes))
            return false;

    } else {
        // Write raw data
        for (int y = 0; y < m_spec.height; ++y) {
            unsigned char* scanline_data
                = &m_uncompressed_image[y * scanline_bytes];
            if (!write_scanline_raw(y, scanline_data))
                return false;
        }
    }

    return true;
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
        // We've been emulating tiles; now dump as scanlines.
        OIIO_ASSERT(m_uncompressed_image.size());
        ok &= write_scanlines(m_spec.y, m_spec.y + m_spec.height, 0,
                              m_spec.format, &m_uncompressed_image[0]);
    }

    // If we want RLE encoding or we were tiled, output all the processed scanlines now.
    if (ok && (m_want_rle || m_spec.tile_width)) {
        ok &= write_buffered_pixels();
    }

    m_uncompressed_image.clear();
    m_uncompressed_image.shrink_to_fit();

    init();

    return ok;
}



bool
SgiOutput::create_and_write_header()
{
    sgi_pvt::SgiHeader sgi_header;
    sgi_header.magic   = sgi_pvt::SGI_MAGIC;
    sgi_header.storage = m_want_rle ? sgi_pvt::RLE : sgi_pvt::VERBATIM;
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
