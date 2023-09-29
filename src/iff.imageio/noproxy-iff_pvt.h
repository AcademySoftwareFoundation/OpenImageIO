// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#pragma once

// Format reference: Affine Toolkit (Thomas E. Burge), riff.h and riff.c
//                   Autodesk Maya documentation, ilib.h

#include <cstdio>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/imageio.h>

#include "imageio_pvt.h"


OIIO_PLUGIN_NAMESPACE_BEGIN

namespace iff_pvt {

// compression numbers
const uint32_t NONE = 0;
const uint32_t RLE  = 1;
const uint32_t QRL  = 2;
const uint32_t QR4  = 3;

const uint32_t RGB     = 0x00000001;
const uint32_t ALPHA   = 0x00000002;
const uint32_t RGBA    = RGB | ALPHA;
const uint32_t ZBUFFER = 0x00000004;
const uint32_t BLACK   = 0x00000010;

// store information about IFF file
class IffFileHeader {
public:
    // reads information about IFF file
    bool read_header(FILE* fd, std::string& err);

    // header information
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t compression;
    uint8_t pixel_bits;
    uint8_t pixel_channels;
    uint16_t tiles;
    uint16_t tile_width;
    uint16_t tile_height;

    // author string
    std::string author;

    // date string
    std::string date;

    // tbmp start
    uint32_t tbmp_start;

    // for4 start
    uint32_t for4_start;

private:
    // Read a uint32_t, swap endian if necessary
    bool read(FILE* fd, uint32_t& data)
    {
        bool ok = (fread(&data, 1, sizeof(data), fd) == sizeof(data));
        if (littleendian())
            swap_endian(&data);
        return ok;
    }
    // Read a uint16_t, swap endian if necessary
    bool read(FILE* fd, uint16_t& data)
    {
        bool ok = (fread(&data, 1, sizeof(data), fd) == sizeof(data));
        if (littleendian())
            swap_endian(&data);
        return ok;
    }
    // Read a 4-byte code (no endian swap), and if that succeeds (beware of EOF or
    // other errors), then also read a 32 bit size (subject to endian swap).
    bool read_typesize(FILE* fd, uint8_t type[4], uint32_t& size)
    {
        return (fread(type, 1, 4, fd) == 4) && read(fd, size);
    }
};



// align size
inline uint32_t
align_size(uint32_t size, uint32_t alignment)
{
    uint32_t mod = size % alignment;
    if (mod) {
        mod = alignment - mod;
        size += mod;
    }
    return size;
}

// tile width
inline const int&
tile_width()
{
    static int tile_w = 64;
    return tile_w;
}

// tile height
inline const int&
tile_height()
{
    static int tile_h = 64;
    return tile_h;
}

// tile width size
inline uint32_t
tile_width_size(uint32_t width)
{
    uint32_t tw = tile_width();
    return (width + tw - 1) / tw;
}

// tile height size
inline uint32_t
tile_height_size(uint32_t height)
{
    uint32_t th = tile_height();
    return (height + th - 1) / th;
}

}  // namespace iff_pvt



class IffInput final : public ImageInput {
public:
    IffInput() { init(); }
    ~IffInput() override
    {
        try {
            close();
        } catch (const std::exception& e) {
            OIIO::pvt::errorfmt("{}", e.what());
        }
    }
    const char* format_name(void) const override { return "iff"; }
    bool open(const std::string& name, ImageSpec& spec) override;
    bool close(void) override;
    bool read_native_scanline(int subimage, int miplevel, int y, int z,
                              void* data) override;
    bool read_native_tile(int subimage, int miplevel, int x, int y, int z,
                          void* data) override;

private:
    FILE* m_fd;
    std::string m_filename;
    iff_pvt::IffFileHeader m_iff_header;
    std::vector<uint8_t> m_buf;

    uint32_t m_tbmp_start;

    // init to initialize state
    void init(void)
    {
        m_fd = NULL;
        m_filename.clear();
        m_buf.clear();
    }

    // helper to read an image
    bool readimg(void);

    // helper to uncompress a rle channel
    size_t uncompress_rle_channel(const uint8_t* in, uint8_t* out, int size);

    bool read_short(uint16_t& val)
    {
        bool ok = fread(&val, sizeof(val), 1, m_fd);
        if (littleendian())
            swap_endian(&val);
        return ok;
    }

    bool read_int(uint32_t& val)
    {
        bool ok = fread(&val, sizeof(val), 1, m_fd);
        if (littleendian())
            swap_endian(&val);
        return ok;
    }

    bool read_str(std::string& val, uint32_t len, uint32_t round = 4)
    {
        const uint32_t big = 1024;
        char strbuf[big];
        len     = std::min(len, big);
        bool ok = fread(strbuf, len, 1, m_fd);
        val.assign(strbuf, len);
        for (uint32_t pad = len % round; pad; --pad)
            fgetc(m_fd);
        return ok;
    }

    bool read_type_len(std::string& type, uint32_t& len)
    {
        return read_str(type, 4) && read_int(len);
    }

    bool read_meta_string(std::string& name, std::string& val)
    {
        uint32_t len = 0;
        return read_type_len(name, len) && read_str(val, len);
    }
};



class IffOutput final : public ImageOutput {
public:
    IffOutput() { init(); }
    ~IffOutput() override
    {
        try {
            close();
        } catch (const std::exception& e) {
            OIIO::pvt::errorfmt("{}", e.what());
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
    FILE* m_fd;
    std::string m_filename;
    iff_pvt::IffFileHeader m_iff_header;
    std::vector<uint8_t> m_buf;
    unsigned int m_dither;
    std::vector<uint8_t> scratch;

    void init(void)
    {
        m_fd = NULL;
        m_filename.clear();
    }

    // writes information about iff file to give file
    bool write_header(iff_pvt::IffFileHeader& header);

    bool write_short(uint16_t val)
    {
        if (littleendian())
            swap_endian(&val);
        return fwrite(&val, sizeof(val), 1, m_fd);
    }
    bool write_int(uint32_t val)
    {
        if (littleendian())
            swap_endian(&val);
        return fwrite(&val, sizeof(val), 1, m_fd);
    }

    bool write_str(string_view val, size_t round = 4)
    {
        bool ok = fwrite(val.data(), val.size(), 1, m_fd);
        for (size_t i = val.size(); i < round_to_multiple(val.size(), round);
             ++i)
            ok &= (fputc(' ', m_fd) != EOF);
        return ok;
    }

    bool write_meta_string(string_view name, string_view val,
                           bool write_if_empty = false)
    {
        if (val.empty() && !write_if_empty)
            return true;
        return write_str(name) && write_int(int(val.size()))
               && (val.size() == 0 || write_str(val));
    }

    // helper to compress verbatim
    void compress_verbatim(const uint8_t*& in, uint8_t*& out, int size);

    // helper to compress duplicate
    void compress_duplicate(const uint8_t*& in, uint8_t*& out, int size);

    // helper to compress a rle channel
    size_t compress_rle_channel(const uint8_t* in, uint8_t* out, int size);
};

OIIO_PLUGIN_NAMESPACE_END
