/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/
#ifndef OPENIMAGEIO_SGI_H
#define OPENIMAGEIO_SGI_H

// Format reference: ftp://ftp.sgi.com/graphics/SGIIMAGESPEC

#include <cstdio>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace sgi_pvt {

    // magic number identifying SGI file
    const short SGI_MAGIC = 0x01DA;

    // SGI file header - all fields are written in big-endian to file
    struct SgiHeader {
        int16_t magic;       // must be 0xDA01 (big-endian)
        int8_t storage;      // compression used, see StorageFormat enum
        int8_t bpc;          // number of bytes per pixel channel
        uint16_t dimension;  // dimension of he image, see Dimension
        uint16_t xsize;      // width in pixels
        uint16_t ysize;      // height in pixels
        uint16_t zsize;      // number of channels: 1(B/W), 3(RGB) or 4(RGBA)
        int32_t pixmin;      // minimum pixel value
        int32_t pixmax;      // maximum pixel value
        int32_t dummy;       // unused, should be set to 0
        char imagename[80];  // null terminated ASCII string
        int32_t colormap;    // how pixels should be interpreted
                             // see ColorMap enum
    };

    // size of the header with all dummy bytes
    const int SGI_HEADER_LEN = 512;

    enum StorageFormat {
        VERBATIM= 0,    // uncompressed
        RLE             // RLE compressed
    };

    enum Dimension {
        ONE_SCANLINE_ONE_CHANNEL = 1, // single scanline and single channel
        MULTI_SCANLINE_ONE_CHANNEL,   // multiscanline, single channel
        MULTI_SCANLINE_MULTI_CHANNEL  // multiscanline, multichannel
    };

    enum ColorMap {
        NORMAL = 0,  // B/W image for 1 channel, RGB for 3 channels and RGBA for 4
        DITHERED,    // only one channel of data, RGB values are packed int one byte:
                     // red and gree - 3 bits, blue - 2 bits; obsolete
        SCREEN,      // obsolete
        COLORMAP     // TODO: what is this?
    };

} // namespace sgi_pvt



class SgiInput final : public ImageInput {
 public:
    SgiInput () { init(); }
    virtual ~SgiInput () { close(); }
    virtual const char *format_name (void) const { return "sgi"; }
    virtual bool valid_file (const std::string &filename) const;
    virtual bool open (const std::string &name, ImageSpec &spec);
    virtual bool close (void);
    virtual bool read_native_scanline (int y, int z, void *data);
 private:
    FILE *m_fd;
    std::string m_filename;
    sgi_pvt::SgiHeader m_sgi_header;
    std::vector<uint32_t> start_tab;
    std::vector<uint32_t> length_tab;

    void init() {
        m_fd = NULL;
        memset (&m_sgi_header, 0, sizeof(m_sgi_header));
    }

    // reads SGI file header (512 bytes) into m_sgi_header
    // Return true if ok, false if there was a read error.
    bool read_header();

    // reads RLE scanline start offset and RLE scanline length tables
    // RLE scanline start offset is stored in start_tab
    // RLE scanline length is stored in length_tab
    // Return true if ok, false if there was a read error.
    bool read_offset_tables();

    // read channel scanline data from file, uncompress it and save the data to
    // 'out' buffer; 'out' should be allocate before call to this method.
    // Return true if ok, false if there was a read error.
    bool uncompress_rle_channel (int scanline_off, int scanline_len,
                                 unsigned char *out);

    /// Helper: read, with error detection
    ///
    bool fread (void *buf, size_t itemsize, size_t nitems) {
        size_t n = ::fread (buf, itemsize, nitems, m_fd);
        if (n != nitems)
            error ("Read error");
        return n == nitems;
    }
};



class SgiOutput final : public ImageOutput {
 public:
    SgiOutput () : m_fd(NULL) { }
    virtual ~SgiOutput () { close(); }
    virtual const char *format_name (void) const { return "sgi"; }
    virtual int supports (string_view feature) const;
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool close (void);
    virtual bool write_scanline (int y, int z, TypeDesc format, const void *data,
                                 stride_t xstride);
    virtual bool write_tile (int x, int y, int z, TypeDesc format,
                             const void *data, stride_t xstride,
                             stride_t ystride, stride_t zstride);
 private:
    FILE *m_fd;
    std::string m_filename;
    std::vector<unsigned char> m_scratch;
    unsigned int m_dither;
    std::vector<unsigned char> m_tilebuffer;

    void init () {
        m_fd = NULL;
    }

    bool create_and_write_header();

    /// Helper - write, with error detection
    template <class T>
    bool fwrite (const T *buf, size_t itemsize=sizeof(T), size_t nitems=1) {
        size_t n = std::fwrite (buf, itemsize, nitems, m_fd);
        if (n != nitems)
            error ("Error writing \"%s\" (wrote %d/%d records)", m_filename, (int)n, (int)nitems);
        return n == nitems;
    }
};


OIIO_PLUGIN_NAMESPACE_END

#endif // OPENIMAGEIO_SGI_H

