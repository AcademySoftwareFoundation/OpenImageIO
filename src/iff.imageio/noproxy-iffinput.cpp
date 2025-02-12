// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO
#include "noproxy-iff_pvt.h"

#include <cmath>

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace iff_pvt;

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

    m_fd = Filesystem::fopen(m_filename, "rb");
    if (!m_fd) {
        errorfmt("Could not open file \"{}\"", name);
        return false;
    }

    // we read header of the file that we think is IFF file
    std::string err;
    if (!m_iff_header.read_header(m_fd, err)) {
        errorfmt("\"{}\": could not read iff header ({})", m_filename,
                 err.size() ? err : std::string("unknown"));
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
    if (m_iff_header.tile_width > 0 || m_iff_header.tile_height > 0) {
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
    if (m_fd) {
        fclose(m_fd);
        m_fd = NULL;
    }
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
    fseek(m_fd, m_tbmp_start, SEEK_SET);

    // resize buffer
    m_buf.resize(m_spec.image_bytes());

    for (unsigned int t = 0; t < m_iff_header.tiles;) {
        // get type
        if (!fread(&type, 1, sizeof(type), m_fd) ||
            // get length
            !fread(&size, 1, sizeof(size), m_fd))
            return false;

        if (littleendian())
            swap_endian(&size);

        chunksize = align_size(size, 4);

        // check if RGBA
        if (type[0] == 'R' && type[1] == 'G' && type[2] == 'B'
            && type[3] == 'A') {
            // get tile coordinates.
            uint16_t xmin, xmax, ymin, ymax;
            if (!fread(&xmin, 1, sizeof(xmin), m_fd)
                || !fread(&ymin, 1, sizeof(ymin), m_fd)
                || !fread(&xmax, 1, sizeof(xmax), m_fd)
                || !fread(&ymax, 1, sizeof(ymax), m_fd))
                return false;

            // swap endianness
            if (littleendian()) {
                swap_endian(&xmin);
                swap_endian(&ymin);
                swap_endian(&xmax);
                swap_endian(&ymax);
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

                if (!fread(&scratch[0], 1, scratch.size(), m_fd))
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

                if (!fread(&scratch[0], 1, scratch.size(), m_fd))
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
            if (fseek(m_fd, chunksize, SEEK_CUR))
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
