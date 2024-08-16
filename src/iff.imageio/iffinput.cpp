// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO
#include "iff_pvt.h"

#include <cmath>

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace iff_pvt;

class IffInput final : public ImageInput {
public:
    IffInput() { init(); }
    ~IffInput() override { close(); }
    const char* format_name(void) const override { return "iff"; }
    int supports(string_view feature) const override
    {
        return feature == "ioproxy";
    }
    bool open(const std::string& name, ImageSpec& spec) override;
    bool open(const std::string& name, ImageSpec& newspec,
              const ImageSpec& config) override;
    bool close(void) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool read_native_tile(int subimage, int miplevel, int x, int y, int z,
                          void* data) override;

private:
    std::string m_filename;
    iff_pvt::IffFileHeader m_iff_header;
    std::vector<uint8_t> m_buf;

    uint32_t m_tbmp_start;

    // init to initialize state
    void init(void)
    {
        ioproxy_clear();
        m_filename.clear();
        m_buf.clear();
    }

    // Reads information about IFF file. If errors are encountereed,
    // read_header wil. issue error messages and return false.
    bool read_header();

    // helper to read an image
    bool readimg(void);

    // helper to uncompress a rle channel
    size_t uncompress_rle_channel(const uint8_t* in, uint8_t* out, int size);

    /// Helper: read buf[0..nitems-1], swap endianness if necessary
    template<typename T> bool read(T* buf, size_t nitems = 1)
    {
        if (!ioread(buf, sizeof(T), nitems))
            return false;
        if (littleendian()
            && (std::is_same<T, uint16_t>::value
                || std::is_same<T, int16_t>::value
                || std::is_same<T, uint32_t>::value
                || std::is_same<T, int32_t>::value)) {
            swap_endian(buf, nitems);
        }
        return true;
    }

    bool read_str(std::string& val, uint32_t len, uint32_t round = 4)
    {
        const uint32_t big = 1024;
        char strbuf[big];
        len     = std::min(len, big);
        bool ok = ioread(strbuf, len);
        val.assign(strbuf, len);
        ok &= ioseek(len % round, SEEK_CUR);
        return ok;
    }

    bool read_type_len(std::string& type, uint32_t& len)
    {
        return read_str(type, 4) && read(&len);
    }

    bool read_meta_string(std::string& name, std::string& val)
    {
        uint32_t len = 0;
        return read_type_len(name, len) && read_str(val, len);
    }

    // Read a 4-byte type code (no endian swap), and if that succeeds (beware
    // of EOF or other errors), then also read a 32 bit size (subject to
    // endian swap).
    bool read_typesize(uint8_t type[4], uint32_t& size)
    {
        return ioread(type, 1, 4) && read(&size);
    }
};



// Obligatory material to make this a recognizable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT int iff_imageio_version = OIIO_PLUGIN_VERSION;

OIIO_EXPORT const char*
iff_imageio_library_version()
{
    return nullptr;
}

OIIO_EXPORT ImageInput*
iff_input_imageio_create()
{
    return new IffInput;
}

OIIO_EXPORT const char* iff_input_extensions[] = { "iff", "z", nullptr };

OIIO_PLUGIN_EXPORTS_END



bool
IffInput::open(const std::string& name, ImageSpec& newspec,
               const ImageSpec& config)
{
    // Check 'config' for any special requests
    ioproxy_retrieve_from_config(config);
    return open(name, newspec);
}



bool
IffInput::open(const std::string& name, ImageSpec& spec)
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

    // saving 'name' for later use
    m_filename = name;

    if (!ioproxy_use_or_open(name))
        return false;
    ioseek(0);

    // we read header of the file that we think is IFF file
    if (!read_header()) {
        close();
        return false;
    }

    // image specification
    m_spec = ImageSpec(m_iff_header.width, m_iff_header.height,
                       m_iff_header.pixel_channels,
                       m_iff_header.pixel_bits == 8 ? TypeDesc::UINT8
                                                    : TypeDesc::UINT16);
    // set x, y
    m_spec.x = m_iff_header.x;
    m_spec.y = m_iff_header.y;

    // set full width, height
    m_spec.full_width  = m_iff_header.width;
    m_spec.full_height = m_iff_header.height;

    // tiles
    if (m_iff_header.tile_width > 0 && m_iff_header.tile_height > 0) {
        m_spec.tile_width  = m_iff_header.tile_width;
        m_spec.tile_height = m_iff_header.tile_height;
        // only 1 subimage for IFF
        m_spec.tile_depth = 1;
    } else {
        errorfmt("\"{}\": wrong tile size", m_filename);
        close();
        return false;
    }

    // attributes

    // compression
    if (m_iff_header.compression == iff_pvt::RLE) {
        m_spec.attribute("compression", "rle");
    }

    // author
    if (m_iff_header.author.size()) {
        m_spec.attribute("Artist", m_iff_header.author);
    }

    // date
    if (m_iff_header.date.size()) {
        m_spec.attribute("DateTime", m_iff_header.date);
    }

    // file pointer is set to the beginning of tbmp data
    // we save this position - it will be helpful in read_native_tile
    m_tbmp_start = m_iff_header.tbmp_start;

    spec = m_spec;
    return true;
}



bool
IffInput::read_header()
{
    uint8_t type[4];
    uint32_t size;
    uint32_t chunksize;
    uint32_t tbhdsize;
    uint32_t flags;
    uint16_t bytes;
    uint16_t prnum;
    uint16_t prden;

    // read FOR4 <size> CIMG.
    for (;;) {
        // get type and length
        if (!read_typesize(type, size))
            return false;

        chunksize = align_size(size, 4);

        if (type[0] == 'F' && type[1] == 'O' && type[2] == 'R'
            && type[3] == '4') {
            // get type
            if (!ioread(&type, 1, sizeof(type)))
                return false;

            // check if CIMG
            if (type[0] == 'C' && type[1] == 'I' && type[2] == 'M'
                && type[3] == 'G') {
                // read TBHD.
                for (;;) {
                    if (!read_typesize(type, size))
                        return false;

                    chunksize = align_size(size, 4);

                    if (type[0] == 'T' && type[1] == 'B' && type[2] == 'H'
                        && type[3] == 'D') {
                        tbhdsize = size;

                        // test if table header size is correct
                        if (tbhdsize != 24 && tbhdsize != 32) {
                            errorfmt("Bad table ehader size {}", tbhdsize);
                            return false;  // bad table header
                        }

                        // get width and height
                        if (!read(&m_iff_header.width)
                            || !read(&m_iff_header.height) || !read(&prnum)
                            || !read(&prden) || !read(&flags) || !read(&bytes)
                            || !read(&m_iff_header.tiles)
                            || !read(&m_iff_header.compression)) {
                            return false;
                        }

                        // get xy
                        if (tbhdsize == 32) {
                            if (!read(&m_iff_header.x)
                                || !read(&m_iff_header.y)) {
                                return false;
                            }
                        } else {
                            m_iff_header.x = 0;
                            m_iff_header.y = 0;
                        }

                        // tiles
                        if (m_iff_header.tiles == 0) {
                            errorfmt("non-tiles are not supported");
                            return false;
                        }

                        // 0 no compression
                        // 1 RLE compression
                        // 2 QRL (not supported)
                        // 3 QR4 (not supported)
                        if (m_iff_header.compression > 1) {
                            errorfmt("only RLE compression is supported");
                            return false;
                        }

                        // test format.
                        if (flags & RGBA) {
                            // test if black is set
                            OIIO_DASSERT(!(flags & BLACK));

                            // test for RGB channels.
                            if (flags & RGB)
                                m_iff_header.pixel_channels = 3;

                            // test for alpha channel
                            if (flags & ALPHA)
                                m_iff_header.pixel_channels++;

                            // test pixel bits
                            m_iff_header.pixel_bits = bytes ? 16 : 8;
                        }

                        // Z format.
                        else if (flags & ZBUFFER) {
                            m_iff_header.pixel_channels = 1;
                            m_iff_header.pixel_bits     = 32;  // 32bit
                            // NOTE: Z_F32 support - not supported
                            OIIO_DASSERT(bytes == 0);
                        }

                        // read AUTH, DATE or FOR4

                        for (;;) {
                            // get type
                            if (!read_typesize(type, size))
                                return false;

                            chunksize = align_size(size, 4);

                            if (type[0] == 'A' && type[1] == 'U'
                                && type[2] == 'T' && type[3] == 'H') {
                                std::vector<char> str(chunksize);
                                if (!ioread(&str[0], 1, chunksize))
                                    return false;
                                m_iff_header.author = std::string(&str[0],
                                                                  size);
                            } else if (type[0] == 'D' && type[1] == 'A'
                                       && type[2] == 'T' && type[3] == 'E') {
                                std::vector<char> str(chunksize);
                                if (!ioread(&str[0], 1, chunksize))
                                    return false;
                                m_iff_header.date = std::string(&str[0], size);
                            } else if (type[0] == 'F' && type[1] == 'O'
                                       && type[2] == 'R' && type[3] == '4') {
                                if (!ioread(&type, 1, sizeof(type)))
                                    return false;

                                // check if CIMG
                                if (type[0] == 'T' && type[1] == 'B'
                                    && type[2] == 'M' && type[3] == 'P') {
                                    // tbmp position for later user in in
                                    // read_native_tile

                                    m_iff_header.tbmp_start = iotell();

                                    // read first RGBA block to detect tile size.

                                    for (unsigned int t = 0;
                                         t < m_iff_header.tiles; t++) {
                                        if (!read_typesize(type, size))
                                            return false;
                                        chunksize = align_size(size, 4);

                                        // check if RGBA
                                        if (type[0] == 'R' && type[1] == 'G'
                                            && type[2] == 'B'
                                            && type[3] == 'A') {
                                            // get tile coordinates.
                                            uint16_t xmin, xmax, ymin, ymax;
                                            if (!read(&xmin) || !read(&ymin)
                                                || !read(&xmax) || !read(&ymax))
                                                return false;

                                            // check tile
                                            if (xmin > xmax || ymin > ymax
                                                || xmax >= m_iff_header.width
                                                || ymax >= m_iff_header.height)
                                                return false;

                                            // set tile width and height
                                            m_iff_header.tile_width
                                                = xmax - xmin + 1;
                                            m_iff_header.tile_height
                                                = ymax - ymin + 1;

                                            // done, return
                                            return true;
                                        }

                                        // skip to the next block.
                                        if (!ioseek(chunksize, SEEK_CUR))
                                            return false;
                                    }
                                } else {
                                    // skip to the next block.
                                    if (!ioseek(chunksize, SEEK_CUR))
                                        return false;
                                }
                            } else {
                                // skip to the next block.
                                if (!ioseek(chunksize, SEEK_CUR))
                                    return false;
                            }
                        }
                        // TBHD done, break
                        break;
                    }

                    // skip to the next block.
                    if (!ioseek(chunksize, SEEK_CUR))
                        return false;
                }
            }
        }
        // skip to the next block.
        if (!ioseek(chunksize, SEEK_CUR))
            return false;
    }
    errorfmt("unknown error reading header");
    return false;
}



bool
IffInput::read_native_scanline(int /*subimage*/, int /*miplevel*/, int /*y*/,
                               int /*z*/, void* /*data*/)
{
    // scanline not used for Maya IFF, uses tiles instead.
    return false;
}



bool
IffInput::read_native_tile(int subimage, int miplevel, int x, int y, int /*z*/,
                           void* data)
{
    lock_guard lock(*this);
    if (!seek_subimage(subimage, miplevel))
        return false;

    if (m_buf.empty())
        readimg();

    // tile size
    int w  = m_spec.width;
    int tw = std::min(x + m_spec.tile_width, m_spec.width) - x;
    int th = std::min(y + m_spec.tile_height, m_spec.height) - y;

    // tile data
    int oy = 0;
    for (int iy = y; iy < y + th; iy++) {
        // in
        uint8_t* in_p = &m_buf[0] + (iy * w + x) * m_spec.pixel_bytes();
        // out
        uint8_t* out_p = (uint8_t*)data
                         + (oy * m_spec.tile_width) * m_spec.pixel_bytes();
        // copy
        memcpy(out_p, in_p, tw * m_spec.pixel_bytes());
        oy++;
    }

    return true;
}



bool inline IffInput::close(void)
{
    init();
    return true;
}



bool
IffInput::readimg()
{
    uint8_t type[4];
    uint32_t size;
    uint32_t chunksize;

    // seek pos
    // set position tile may be called randomly
    ioseek(m_tbmp_start);

    // resize buffer
    m_buf.resize(m_spec.image_bytes());

    for (unsigned int t = 0; t < m_iff_header.tiles;) {
        // get type and length
        if (!ioread(&type, 1, sizeof(type)) || !read(&size))
            return false;

        chunksize = align_size(size, 4);

        // check if RGBA
        if (type[0] == 'R' && type[1] == 'G' && type[2] == 'B'
            && type[3] == 'A') {
            // get tile coordinates.
            uint16_t xmin, xmax, ymin, ymax;
            if (!read(&xmin) || !read(&ymin) || !read(&xmax) || !read(&ymax))
                return false;

            // get tile width/height
            uint32_t tw = xmax - xmin + 1;
            uint32_t th = ymax - ymin + 1;

            // get image size
            // skip coordinates, uint16_t (2) * 4 = 8
            uint32_t image_size = chunksize - 8;

            // check tile
            if (xmin > xmax || ymin > ymax || xmax >= m_spec.width
                || ymax >= m_spec.height || !tw || !th) {
                return false;
            }

            // tile compress
            bool tile_compress = false;

            // if tile compression fails to be less than image data stored
            // uncompressed the tile is written uncompressed

            // set channels
            uint8_t channels = m_iff_header.pixel_channels;

            // set tile size
            uint32_t tile_size = tw * th * channels * m_spec.channel_bytes()
                                 + 8;

            // test if compressed
            // we use the non aligned size
            if (tile_size > size) {
                tile_compress = true;
            }

            // handle 8-bit data.
            if (m_iff_header.pixel_bits == 8) {
                std::vector<uint8_t> scratch;

                // set bytes.
                scratch.resize(image_size);

                if (!ioread(scratch.data(), 1, scratch.size()))
                    return false;

                // set tile data
                uint8_t* p = static_cast<uint8_t*>(&scratch[0]);

                // tile compress.
                if (tile_compress) {
                    // map BGR(A) to RGB(A)
                    for (int c = (channels * m_spec.channel_bytes()) - 1;
                         c >= 0; --c) {
                        std::vector<uint8_t> in(tw * th);
                        uint8_t* in_p = &in[0];

                        // uncompress and increment
                        p += uncompress_rle_channel(p, in_p, tw * th);

                        // set tile
                        for (uint16_t py = ymin; py <= ymax; py++) {
                            uint8_t* out_dy = static_cast<uint8_t*>(&m_buf[0])
                                              + (py * m_spec.width)
                                                    * m_spec.pixel_bytes();

                            for (uint16_t px = xmin; px <= xmax; px++) {
                                uint8_t* out_p
                                    = out_dy + px * m_spec.pixel_bytes() + c;
                                *out_p++ = *in_p++;
                            }
                        }
                    }
                } else {
                    int sy = 0;
                    for (uint16_t py = ymin; py <= ymax; py++) {
                        uint8_t* out_dy = static_cast<uint8_t*>(&m_buf[0])
                                          + (py * m_spec.width + xmin)
                                                * m_spec.pixel_bytes();

                        // set tile
                        int sx = 0;
                        for (uint16_t px = xmin; px <= xmax; px++) {
                            uint8_t* in_p
                                = p + (sy * tw + sx) * m_spec.pixel_bytes();

                            // map BGR(A) to RGB(A)
                            for (int c = channels - 1; c >= 0; --c) {
                                uint8_t* out_p = in_p
                                                 + (c * m_spec.channel_bytes());
                                *out_dy++ = *out_p;
                            }
                            sx++;
                        }
                        sy++;
                    }
                }
            }
            // handle 16-bit data.
            else if (m_iff_header.pixel_bits == 16) {
                std::vector<uint8_t> scratch;

                // set bytes.
                scratch.resize(image_size);

                if (!ioread(scratch.data(), 1, scratch.size()))
                    return false;

                // set tile data
                uint8_t* p = static_cast<uint8_t*>(&scratch[0]);

                if (tile_compress) {
                    // set map
                    std::vector<uint8_t> map;
                    if (littleendian()) {
                        int rgb16[]  = { 0, 2, 4, 1, 3, 5 };
                        int rgba16[] = { 0, 2, 4, 6, 1, 3, 5, 7 };
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

                    // map BGR(A)BGR(A) to RRGGBB(AA)
                    for (int c = (channels * m_spec.channel_bytes()) - 1;
                         c >= 0; --c) {
                        int mc = map[c];

                        std::vector<uint8_t> in(tw * th);
                        uint8_t* in_p = &in[0];

                        // uncompress and increment
                        p += uncompress_rle_channel(p, in_p, tw * th);

                        // set tile
                        for (uint16_t py = ymin; py <= ymax; py++) {
                            uint8_t* out_dy = static_cast<uint8_t*>(&m_buf[0])
                                              + (py * m_spec.width)
                                                    * m_spec.pixel_bytes();

                            for (uint16_t px = xmin; px <= xmax; px++) {
                                uint8_t* out_p
                                    = out_dy + px * m_spec.pixel_bytes() + mc;
                                *out_p++ = *in_p++;
                            }
                        }
                    }
                } else {
                    int sy = 0;
                    for (uint16_t py = ymin; py <= ymax; py++) {
                        uint8_t* out_dy = static_cast<uint8_t*>(&m_buf[0])
                                          + (py * m_spec.width + xmin)
                                                * m_spec.pixel_bytes();

                        // set scanline, make copy easier
                        std::vector<uint16_t> scanline(tw
                                                       * m_spec.pixel_bytes());
                        uint16_t* sl_p = &scanline[0];

                        // set tile
                        int sx = 0;
                        for (uint16_t px = xmin; px <= xmax; px++) {
                            uint8_t* in_p
                                = p + (sy * tw + sx) * m_spec.pixel_bytes();

                            // map BGR(A) to RGB(A)
                            for (int c = channels - 1; c >= 0; --c) {
                                uint16_t pixel;
                                uint8_t* out_p = in_p
                                                 + (c * m_spec.channel_bytes());
                                memcpy(&pixel, out_p, 2);
                                // swap endianness
                                if (littleendian()) {
                                    swap_endian(&pixel);
                                }
                                *sl_p++ = pixel;
                            }
                            sx++;
                        }
                        // copy data
                        memcpy(out_dy, &scanline[0], tw * m_spec.pixel_bytes());
                        sy++;
                    }
                }

            } else {
                errorfmt("\"{}\": unsupported number of bits per pixel for tile",
                         m_filename);
                return false;
            }

            // tile
            t++;

        } else {
            // skip to the next block
            if (!ioseek(chunksize))
                return false;
        }
    }

    // flip buffer to make read_native_tile easier,
    // from tga.imageio:

    int bytespp = m_spec.pixel_bytes();

    std::vector<unsigned char> flip(m_spec.width * bytespp);
    unsigned char *src, *dst, *tmp = &flip[0];
    for (int y = 0; y < m_spec.height / 2; y++) {
        src = &m_buf[(m_spec.height - y - 1) * m_spec.width * bytespp];
        dst = &m_buf[y * m_spec.width * bytespp];

        memcpy(tmp, src, m_spec.width * bytespp);
        memcpy(src, dst, m_spec.width * bytespp);
        memcpy(dst, tmp, m_spec.width * bytespp);
    }

    return true;
}



size_t
IffInput::uncompress_rle_channel(const uint8_t* in, uint8_t* out, int size)
{
    const uint8_t* const _in = in;
    const uint8_t* const end = out + size;

    while (out < end) {
        // information.
        const uint8_t count = (*in & 0x7f) + 1;
        const bool run      = (*in & 0x80) ? true : false;
        ++in;

        // find runs
        if (!run) {
            // verbatim
            for (int i = 0; i < count; i++)
                *out++ = *in++;
        } else {
            // duplicate
            const uint8_t p = *in++;
            for (int i = 0; i < count; i++)
                *out++ = p;
        }
    }
    const size_t r = in - _in;
    return r;
}

OIIO_PLUGIN_NAMESPACE_END
