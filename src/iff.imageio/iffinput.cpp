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
    iff_pvt::IffFileHeader m_header;
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
    size_t uncompress_rle_channel(span<uint8_t> in, span<uint8_t> out,
                                  size_t max);

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
    bool read_type(uint8_t type[4]) { return ioread(type, 1, 4); }

    bool read_chunk(uint8_t type[4], uint32_t& size)
    {
        return read_type(type) && read(&size);
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

constexpr uint8_t iff_auth_tag[4] = { 'A', 'U', 'T', 'H' };
constexpr uint8_t iff_date_tag[4] = { 'D', 'A', 'T', 'E' };
constexpr uint8_t iff_for4_tag[4] = { 'F', 'O', 'R', '4' };
constexpr uint8_t iff_cimg_tag[4] = { 'C', 'I', 'M', 'G' };
constexpr uint8_t iff_rgba_tag[4] = { 'R', 'G', 'B', 'A' };
constexpr uint8_t iff_tbhd_tag[4] = { 'T', 'B', 'H', 'D' };
constexpr uint8_t iff_tbmp_tag[4] = { 'T', 'B', 'M', 'P' };
constexpr uint8_t iff_zbuf_tag[4] = { 'Z', 'B', 'U', 'F' };

void
print_header(const IffFileHeader& header)
{
    print("x: {}\n", header.x);
    print("y: {}\n", header.y);
    print("width: {}\n", header.width);
    print("height: {}\n", header.height);
    print("compression: {}\n", header.compression);
    print("rgba_bits: {}\n", header.rgba_bits);
    print("rgba_count: {}\n", header.rgba_count);
    print("tiles: {}\n", header.tiles);
    print("tile_width: {}\n", header.tile_width);
    print("tile_height: {}\n", header.tile_height);
    print("zbuffer: {}\n", header.zbuffer);
    print("zbuffer_bits: {}\n", header.zbuffer_bits);
    print("author: {}\n", header.author);
    print("date: {}\n", header.date);
    print("tbmp_start: {}\n", header.tbmp_start);
    print("for4_start: {}\n", header.for4_start);
    print("channel_bytes(): {}\n", header.channel_bytes());
    print("zbuffer_bytes(): {}\n", header.zbuffer_bytes());
    print("pixel_bytes(): {}\n", header.pixel_bytes());
    print("image_bytes(): {}\n", header.image_bytes());
}

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
    //    FOR4 <size> TBMP ...
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
        errorfmt("IFF error could not read header");
        close();
        return false;
    }

    // set types and channels
    TypeDesc type    = (m_header.rgba_bits == 8) ? TypeDesc::UINT8
                                                 : TypeDesc::UINT16;
    int num_channels = m_header.rgba_count + (m_header.zbuffer ? 1 : 0);

    m_spec = ImageSpec(m_header.width, m_header.height, num_channels, type);

    if (m_header.zbuffer) {
        m_spec.channelformats.assign(num_channels, type);
        m_spec.channelformats.back() = TypeDesc::FLOAT;
        m_spec.channelnames          = { "R", "G", "B" };
        if (m_header.rgba_count == 4) {
            m_spec.alpha_channel = m_spec.channelnames.size();
            m_spec.channelnames.push_back("A");
        }
        m_spec.z_channel = m_spec.channelnames.size();
        m_spec.channelnames.push_back("Z");
    }

    // set x, y
    m_spec.x = m_header.x;
    m_spec.y = m_header.y;

    // set full width, height
    m_spec.full_width  = m_header.width;
    m_spec.full_height = m_header.height;

    // tiles
    if (m_header.tile_width > 0 && m_header.tile_height > 0) {
        m_spec.tile_width  = m_header.tile_width;
        m_spec.tile_height = m_header.tile_height;
        // only 1 subimage for IFF
        m_spec.tile_depth = 1;
    } else {
        errorfmt("\"{}\": wrong tile size", m_filename);
        close();
        return false;
    }

    // attributes

    // compression
    if (m_header.compression == iff_pvt::RLE) {
        m_spec.attribute("compression", "rle");
    }

    // author
    if (m_header.author.size()) {
        m_spec.attribute("Artist", m_header.author);
    }

    // date
    if (m_header.date.size()) {
        m_spec.attribute("DateTime", m_header.date);
    }

    // file pointer is set to the beginning of tbmp data
    // we save this position - it will be helpful in read_native_tile
    m_tbmp_start = m_header.tbmp_start;

    spec = m_spec;
    return true;
}



bool
IffInput::read_header()
{
    uint8_t chunktype[4];
    uint32_t size;
    uint32_t chunksize;
    uint32_t flags;
    uint16_t bytes;
    uint16_t prnum;
    uint16_t prden;

    // read FOR4 <size> CIMG.
    for (;;) {
        if (!read_chunk(chunktype, size))
            return false;

        chunksize = align_chunk(size, 4);

        // chunk type: FOR4
        if (std::memcmp(chunktype, iff_for4_tag, 4) == 0) {
            if (!ioread(&chunktype, 1, sizeof(chunktype))) {
                errorfmt("IFF error io seek failed for type");
                return false;
            }

            // chunk type: CIMG
            if (std::memcmp(chunktype, iff_cimg_tag, 4) == 0) {
                for (;;) {
                    if (!read_chunk(chunktype, size))
                        return false;

                    chunksize = align_chunk(size, 4);

                    // chunk type: TBHD
                    if (std::memcmp(chunktype, iff_tbhd_tag, 4) == 0) {
                        // test if table header size is correct
                        if (size != 24 && size != 32) {
                            errorfmt("IFF error Bad table header size {}",
                                     size);
                            return false;  // bad table header
                        }

                        // get width and height
                        if (!read(&m_header.width) || !read(&m_header.height)
                            || !read(&prnum) || !read(&prden) || !read(&flags)
                            || !read(&bytes) || !read(&m_header.tiles)
                            || !read(&m_header.compression)) {
                            return false;
                        }

                        // get xy
                        if (size == 32) {
                            if (!read(&m_header.x) || !read(&m_header.y)) {
                                return false;
                            }
                        } else {
                            m_header.x = 0;
                            m_header.y = 0;
                        }

                        // tiles
                        if (m_header.tiles == 0) {
                            errorfmt("IFF error non-tiles are not supported");
                            return false;
                        }

                        // 0 no compression
                        // 1 RLE compression
                        // 2 QRL (not supported)
                        // 3 QR4 (not supported)
                        if (m_header.compression > 1) {
                            errorfmt(
                                "IFF error only RLE compression is supported");
                            return false;
                        }

                        // RGB(A) format
                        if (flags & RGBA) {
                            // test if black is set
                            OIIO_DASSERT(!(flags & BLACK));

                            if (flags & RGB)
                                m_header.rgba_count = 3;

                            if (flags & ALPHA)
                                m_header.rgba_count++;

                            m_header.rgba_bits = bytes ? 16 : 8;

                            if (flags & ZBUFFER)
                                m_header.zbuffer = 1;

                            m_header.zbuffer_bits = 32;
                        }
                        // Z format.
                        else if (flags & ZBUFFER) {
                            // todo: we have not seen a sample of this
                            m_header.rgba_count = 1;
                            m_header.rgba_bits  = 32;  // 32bit
                            // NOTE: Z_F32 support - not supported
                            OIIO_DASSERT(bytes == 0);
                        }

                        // read chunks
                        for (;;) {
                            // get type
                            if (!read_chunk(chunktype, size)) {
                                errorfmt("IFF error read type size failed");
                                return false;
                            }

                            chunksize = align_chunk(size, 4);

                            // chunk type: AUTH
                            if (std::memcmp(chunktype, iff_auth_tag, 4) == 0) {
                                std::vector<char> str(chunksize);
                                if (!ioread(str.data(), 1, chunksize))
                                    return false;
                                m_header.author = std::string(str.data(), size);

                                // chunk type: DATE
                            } else if (std::memcmp(chunktype, iff_date_tag, 4)
                                       == 0) {
                                std::vector<char> str(chunksize);
                                if (!ioread(str.data(), 1, chunksize))
                                    return false;
                                m_header.date = std::string(str.data(),
                                                            chunksize);

                                // chunk type: FOR4
                            } else if (std::memcmp(chunktype, iff_for4_tag, 4)
                                       == 0) {
                                if (!ioread(&chunktype, 1, sizeof(chunktype)))
                                    return false;

                                // chunk type: TBMP
                                if (std::memcmp(chunktype, iff_tbmp_tag, 4)
                                    == 0) {
                                    // tbmp position for later user in in
                                    // read_native_tile
                                    m_header.tbmp_start = iotell();

                                    // read first RGBA block to detect tile size.
                                    for (unsigned int t = 0; t < m_header.tiles;
                                         t++) {
                                        if (!read_chunk(chunktype, size))
                                            return false;
                                        chunksize = align_chunk(size, 4);

                                        // chunk type: RGBA
                                        if (std::memcmp(chunktype, iff_rgba_tag,
                                                        4)
                                            == 0) {
                                            // get tile coordinates.
                                            uint16_t xmin, xmax, ymin, ymax;
                                            if (!read(&xmin) || !read(&ymin)
                                                || !read(&xmax) || !read(&ymax))
                                                return false;

                                            // check tile
                                            if (xmin > xmax || ymin > ymax
                                                || xmax >= m_header.width
                                                || ymax >= m_header.height)
                                                return false;

                                            // set tile width and height
                                            m_header.tile_width = xmax - xmin
                                                                  + 1;
                                            m_header.tile_height = ymax - ymin
                                                                   + 1;

                                            // done, return
                                            return true;
                                        } else {
                                            // skip to the next block.
                                            if (!ioseek(chunksize, SEEK_CUR)) {
                                                return false;
                                            }
                                        }
                                    }
                                } else {
                                    // skip to the next block.
                                    if (!ioseek(chunksize, SEEK_CUR)) {
                                        return false;
                                    }
                                }
                            } else {
                                // skip to the next block.
                                if (!ioseek(chunksize, SEEK_CUR)) {
                                    return false;
                                }
                            }
                        }
                        // TBHD done, break
                        break;
                    } else {
                        // skip to the next block.
                        if (!ioseek(chunksize, SEEK_CUR)) {
                            return false;
                        }
                    }
                }
            }
        } else {
            // skip to the next block.
            if (!ioseek(chunksize, SEEK_CUR)) {
                return false;
            }
        }
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

    if (m_buf.empty()) {
        if (!readimg()) {
            return false;
        }
    }

    // tile size
    int w  = m_header.width;
    int tw = std::min(x + static_cast<int>(m_header.tile_width),
                      static_cast<int>(m_header.width))
             - x;
    int th = std::min(y + static_cast<int>(m_header.tile_height),
                      static_cast<int>(m_header.height))
             - y;

    // tile data
    int oy = 0;
    for (int iy = y; iy < y + th; iy++) {
        // in
        uint8_t* in_p = m_buf.data() + (iy * w + x) * m_header.pixel_bytes();
        // out
        uint8_t* out_p = reinterpret_cast<uint8_t*>(data)
                         + (oy * m_header.tile_width) * m_header.pixel_bytes();
        // copy
        memcpy(out_p, in_p, tw * m_header.pixel_bytes());
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
    uint8_t chunktype[4];
    uint32_t chunksize;
    uint32_t size;
    uint16_t rgbatiles = 0;
    uint16_t ztiles    = 0;

    // seek pos
    // set position tile may be called randomly
    ioseek(m_tbmp_start);

    // resize buffer
    m_buf.resize(m_header.image_bytes());

    while ((rgbatiles < m_header.tiles && m_header.rgba_count > 0)
           || (ztiles < m_header.tiles && m_header.zbuffer > 0)) {
        // get type and length
        if (!read_chunk(chunktype, size)) {
            errorfmt("IFF error io could not read rgb(a) type");
            return false;
        }
        chunksize = align_chunk(size, 4);

        // chunk type: RGBA
        if (std::memcmp(chunktype, iff_rgba_tag, 4) == 0) {
            // get tile coordinates.
            uint16_t xmin, xmax, ymin, ymax;
            if (!read(&xmin) || !read(&ymin) || !read(&xmax) || !read(&ymax)) {
                errorfmt("IFF error io read xmin, ymin, xmax and ymax failed");
                return false;
            }

            // get tile width/height
            uint32_t tw = xmax - xmin + 1;
            uint32_t th = ymax - ymin + 1;

            // get image size
            // skip coordinates, uint16_t (2) * 4 = 8
            uint32_t image_size = chunksize - 8;

            // check tile
            if (xmin > xmax || ymin > ymax || xmax >= m_spec.width
                || ymax >= m_spec.height || !tw || !th) {
                errorfmt(
                    "IFF error io xmin, ymin, xmax or ymax does not match");
                return false;
            }

            // tile compress
            bool tile_compressed = false;

            // if tile compression fails to be less than image data stored
            // uncompressed the tile is written uncompressed

            // set tile size
            uint32_t tile_size = tw * th * m_header.rgba_channels_bytes() + 8;

            // test if compressed
            // we use the non aligned size
            if (tile_size > size) {
                tile_compressed = true;
            }

            // handle 8-bit data.
            if (m_header.rgba_bits == 8) {
                std::vector<uint8_t> scratch;
                scratch.resize(image_size);

                if (!ioread(scratch.data(), 1, scratch.size())) {
                    return false;
                }

                span<uint8_t> scratch_span(scratch);

                if (tile_compressed) {
                    for (int c = m_header.rgba_count - 1; c >= 0; --c) {
                        std::vector<uint8_t> in(tw * th);
                        span<uint8_t> in_span(in);

                        size_t used = uncompress_rle_channel(scratch_span,
                                                             in_span, tw * th);
                        if (used > scratch_span.size()) {
                            errorfmt(
                                "RLE uncompress exceeds buffer size for channel: {}",
                                c);
                            return false;
                        }

                        scratch_span = scratch_span.subspan(used);

                        size_t offset = 0;
                        for (uint16_t py = ymin; py <= ymax; ++py) {
                            uint8_t* out_dy = m_buf.data()
                                              + (py * m_header.width)
                                                    * m_header.pixel_bytes();

                            for (uint16_t px = xmin; px <= xmax; ++px) {
                                if (offset >= in_span.size()) {
                                    errorfmt(
                                        "in_span underflow at pixel ({}, {})",
                                        px, py);
                                    return false;
                                }

                                uint8_t* out_p
                                    = out_dy + px * m_header.pixel_bytes() + c;
                                *out_p = in_span[offset++];
                            }
                        }
                    }
                } else {
                    uint8_t* p = scratch.data();
                    span<uint8_t> input(p,
                                        (ymax - ymin + 1) * tw
                                            * m_header.rgba_channels_bytes());

                    int sy = 0;
                    for (uint16_t py = ymin; py <= ymax; ++py, ++sy) {
                        uint8_t* out_dy = m_buf.data()
                                          + (py * m_header.width)
                                                * m_header.pixel_bytes();

                        int sx = 0;
                        for (uint16_t px = xmin; px <= xmax; ++px, ++sx) {
                            size_t offset = (sy * tw + sx)
                                            * m_header.rgba_channels_bytes();

                            if (offset + m_header.rgba_channels_bytes()
                                > input.size()) {
                                errorfmt("input span overflow at ({}, {})", px,
                                         py);
                                return false;
                            }

                            span<uint8_t> pixel_in
                                = input.subspan(offset,
                                                m_header.rgba_channels_bytes());
                            uint8_t* out_p = out_dy
                                             + px * m_header.pixel_bytes();

                            // map BGR(A) to RGB(A)
                            for (int c = m_header.rgba_count - 1; c >= 0; --c) {
                                *out_p++ = pixel_in[c];
                            }
                        }
                    }
                }
            }
            // handle 16-bit data.
            else if (m_header.rgba_bits == 16) {
                std::vector<uint8_t> scratch;
                scratch.resize(image_size);

                if (!ioread(scratch.data(), 1, scratch.size())) {
                    return false;
                }

                span<uint8_t> scratch_span(scratch);

                if (tile_compressed) {
                    std::vector<uint8_t> map;
                    if (littleendian()) {
                        uint8_t rgb16[]  = { 0, 2, 4, 1, 3, 5 };
                        uint8_t rgba16[] = { 0, 2, 4, 6, 1, 3, 5, 7 };
                        map              = (m_header.rgba_count == 3)
                                               ? std::vector<uint8_t>(rgb16, rgb16 + 6)
                                               : std::vector<uint8_t>(rgba16, rgba16 + 8);
                    } else {
                        uint8_t rgb16[]  = { 1, 3, 5, 0, 2, 4 };
                        uint8_t rgba16[] = { 1, 3, 5, 7, 0, 2, 4, 6 };
                        map              = (m_header.rgba_count == 3)
                                               ? std::vector<uint8_t>(rgb16, rgb16 + 6)
                                               : std::vector<uint8_t>(rgba16, rgba16 + 8);
                    }

                    for (int c = m_header.rgba_count * m_header.channel_bytes()
                                 - 1;
                         c >= 0; --c) {
                        int mc = map[c];

                        std::vector<uint8_t> in(tw * th);
                        span<uint8_t> in_span(in);

                        size_t used = uncompress_rle_channel(scratch_span,
                                                             in_span, tw * th);
                        if (used > scratch_span.size()) {
                            errorfmt(
                                "RLE uncompress exceeds span size (channel byte {})",
                                c);
                            return false;
                        }

                        scratch_span = scratch_span.subspan(used);

                        size_t offset = 0;
                        for (uint16_t py = ymin; py <= ymax; ++py) {
                            uint8_t* out_dy = m_buf.data()
                                              + (py * m_header.width)
                                                    * m_header.pixel_bytes();

                            for (uint16_t px = xmin; px <= xmax; ++px) {
                                if (offset >= in_span.size()) {
                                    errorfmt("in_span underflow at ({}, {})",
                                             px, py);
                                    return false;
                                }

                                uint8_t* out_p
                                    = out_dy + px * m_header.pixel_bytes() + mc;
                                *out_p = in_span[offset++];
                            }
                        }
                    }
                } else {
                    uint8_t* p = scratch.data();
                    span<uint8_t> input(p,
                                        (ymax - ymin + 1) * tw
                                            * m_header.rgba_channels_bytes());

                    int sy = 0;
                    for (uint16_t py = ymin; py <= ymax; ++py, ++sy) {
                        uint8_t* out_dy = m_buf.data()
                                          + (py * m_header.width + xmin)
                                                * m_header.pixel_bytes();

                        std::vector<uint16_t> scanline(tw
                                                       * m_header.rgba_count);
                        span<uint16_t> sl_span(scanline);

                        int sx = 0;
                        for (uint16_t px = xmin; px <= xmax; ++px, ++sx) {
                            size_t offset = (sy * tw + sx)
                                            * m_header.rgba_channels_bytes();

                            if (offset + m_header.rgba_channels_bytes()
                                > input.size()) {
                                errorfmt("input span overflow at ({}, {})", px,
                                         py);
                                return false;
                            }

                            span<uint8_t> pixel_in
                                = input.subspan(offset,
                                                m_header.rgba_channels_bytes());

                            for (int c = m_header.rgba_count - 1; c >= 0; --c) {
                                uint16_t pixel;
                                memcpy(&pixel, pixel_in.data() + c * 2, 2);

                                if (littleendian()) {
                                    swap_endian(&pixel);
                                }

                                if (sl_span.empty()) {
                                    errorfmt(
                                        "scanline span overflow at ({}, {})",
                                        px, py);
                                    return false;
                                }

                                sl_span.front() = pixel;
                                sl_span         = sl_span.subspan(1);
                            }
                        }

                        memcpy(out_dy, scanline.data(),
                               tw * m_header.pixel_bytes());
                    }
                }
            } else {
                errorfmt("\"{}\": unsupported number of bits per pixel for tile",
                         m_filename);
                return false;
            }

            rgbatiles++;

            // chunk type: ZBUF
        } else if (std::memcmp(chunktype, iff_zbuf_tag, 4) == 0) {
            // get tile coordinates.
            uint16_t xmin, xmax, ymin, ymax;
            if (!read(&xmin) || !read(&ymin) || !read(&xmax) || !read(&ymax)) {
                errorfmt("IFF error io read xmin, ymin, xmax and ymax failed");
                return false;
            }

            // get tile width/height
            uint32_t tw = xmax - xmin + 1;
            uint32_t th = ymax - ymin + 1;

            // get image size
            // skip coordinates, uint16_t (2) * 4 = 8
            uint32_t image_size = chunksize - 8;

            // check tile
            if (xmin > xmax || ymin > ymax || xmax >= m_spec.width
                || ymax >= m_spec.height || !tw || !th) {
                errorfmt(
                    "IFF error io xmin, ymin, xmax or ymax does not match");
                return false;
            }

            // tile compress
            bool tile_compressed = false;

            // if tile compression fails to be less than image data stored
            // uncompressed the tile is written uncompressed

            // set tile size
            uint32_t tile_size = tw * th * m_header.zbuffer_bytes() + 8;

            // test if compressed
            // we use the non aligned size
            if (tile_size > size) {
                tile_compressed = true;
            }

            // set tile size
            std::vector<uint8_t> scratch;

            // set bytes
            scratch.resize(image_size);

            // zbuffer is always compressed in IFF

            if (!ioread(scratch.data(), 1, scratch.size())) {
                return false;
            }

            // read tile
            if (tile_compressed) {
                span<uint8_t> scratch_span(scratch);

                for (int c = m_header.zbuffer_bytes() - 1; c >= 0; --c) {
                    std::vector<uint8_t> in(tw * th);
                    span<uint8_t> in_span(in);

                    // uncompress and advance span
                    size_t used = uncompress_rle_channel(scratch_span, in_span,
                                                         tw * th);
                    if (used > scratch_span.size()) {
                        errorfmt("rle read exceeds scratch buffer");
                        return false;
                    }
                    scratch_span = scratch_span.subspan(used);

                    // write to output buffer
                    for (uint32_t py = ymin; py <= ymax; py++) {
                        uint8_t* out_dy = static_cast<uint8_t*>(m_buf.data())
                                          + (py * m_header.width)
                                                * m_header.pixel_bytes();

                        for (uint16_t px = xmin; px <= xmax; px++) {
                            uint8_t* out_p = out_dy
                                             + px * m_header.pixel_bytes()
                                             + m_header.rgba_channels_bytes()
                                             + c;

                            if (in_span.empty()) {
                                errorfmt("in span underflow");
                                return false;
                            }
                            *out_p++ = in_span.front();
                            in_span  = in_span.subspan(1);
                        }
                    }
                }

            } else {
                span<uint8_t> scratch_span(scratch);
                size_t total_pixels   = tw * th;
                size_t expected_bytes = total_pixels * m_header.zbuffer_bytes();

                if (scratch_span.size() < expected_bytes) {
                    errorfmt(
                        "scratch buffer too small for uncompressed zbuffer");
                    return false;
                }

                int sy = 0;
                for (uint16_t py = ymin; py <= ymax; py++, sy++) {
                    uint8_t* out_dy = m_buf.data()
                                      + (py * m_header.width)
                                            * m_header.pixel_bytes();

                    int sx = 0;
                    for (uint16_t px = xmin; px <= xmax; px++, sx++) {
                        size_t pixel_index  = sy * tw + sx;
                        size_t pixel_offset = pixel_index
                                              * m_header.zbuffer_bytes();

                        if (pixel_offset + m_header.zbuffer_bytes()
                            > scratch_span.size()) {
                            errorfmt("in span overflow at pixel ({}, {})", px,
                                     py);
                            return false;
                        }

                        span<uint8_t> in_span
                            = scratch_span.subspan(pixel_offset,
                                                   m_header.zbuffer_bytes());
                        uint8_t* out_p = out_dy + px * m_header.pixel_bytes()
                                         + m_header.rgba_channels_bytes();

                        for (int c = m_header.zbuffer_bytes() - 1; c >= 0;
                             --c) {
                            *out_p++ = in_span[c];
                        }
                    }
                }
            }
            ztiles++;

        } else {
            // skip to the next block
            if (!ioseek(chunksize, SEEK_CUR)) {
                return false;
            }
        }
    }

    // flip buffer to make read_native_tile easier,
    // from tga.imageio:

    int bytespp = m_header.pixel_bytes();

    std::vector<unsigned char> flip(m_spec.width * bytespp);
    unsigned char *src, *dst, *tmp = flip.data();
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
IffInput::uncompress_rle_channel(span<uint8_t> in, span<uint8_t> out,
                                 size_t max)
{
    const uint8_t* in_ptr = in.data();
    const uint8_t* in_end = in.data() + in.size();

    uint8_t* out_ptr       = out.data();
    const uint8_t* out_end = out.data() + std::min(max, out.size());

    while (out_ptr < out_end && in_ptr < in_end) {
        if (in_ptr >= in_end)
            break;

        uint8_t header = *in_ptr++;
        uint8_t count  = (header & 0x7f) + 1;
        bool run       = (header & 0x80);

        if (!run) {
            if (in_ptr + count > in_end)
                break;
            if (out_ptr + count > out_end)
                break;

            std::copy(in_ptr, in_ptr + count, out_ptr);
            in_ptr += count;
            out_ptr += count;
        } else {
            if (in_ptr >= in_end)
                break;
            if (out_ptr + count > out_end)
                break;

            uint8_t value = *in_ptr++;
            std::fill(out_ptr, out_ptr + count, value);
            out_ptr += count;
        }
    }
    return static_cast<size_t>(in_ptr - in.data());
}


OIIO_PLUGIN_NAMESPACE_END
