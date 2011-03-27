/*
  Copyright 2008-2009 Larry Gritz and the other authors and contributors.
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
#ifndef OPENIMAGEIO_IFF_H
#define OPENIMAGEIO_IFF_H

// Format reference: Affine Toolkit (Thomas E. Burge), riff.h and riff.c
//                   Autodesk Maya documentation, ilib.h

#include <cstdio>
#include "imageio.h"
#include "fmath.h"


OIIO_PLUGIN_NAMESPACE_BEGIN

namespace iff_pvt {

    // compression numbers
    const uint32_t NONE = 0;
    const uint32_t RLE = 1;
    const uint32_t QRL = 2; 
    const uint32_t QR4 = 3;
    
    const uint32_t RGB = 0x00000001;
    const uint32_t ALPHA = 0x00000002;
    const uint32_t RGBA = RGB | ALPHA;
    const uint32_t ZBUFFER = 0x00000004;
    const uint32_t BLACK = 0x00000010;
    
    // store informations about IFF file
    class IffFileHeader {
    public:
        // reads information about IFF file
        bool read_header (FILE *fd);
        
        // writes information about iff file to give file
        bool write_header (FILE *fd);
          
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
    };
    
    // align size
    inline uint32_t
    align_size (uint32_t size, uint32_t alignment)
    {
        uint32_t mod = size % alignment;
        if (mod)
        {
            mod = alignment - mod;
            size += mod;
        }
        return size;
    }

    // tile width
    inline const int & 
    tile_width()
    {
        static int tile_w = 64;
        return tile_w;
    }

    // tile height
    inline const int & 
    tile_height()
    {
        static int tile_h = 64;
        return tile_h;
    }
    
    // tile width size
    inline uint32_t
    tile_width_size (uint32_t width)
    {
        uint32_t tw = tile_width();
        return (width + tw - 1) / tw;
    }
    
    // tile height size
    inline uint32_t
    tile_height_size (uint32_t height)
    {
        uint32_t th = tile_height();
        return (height + th - 1) / th;
    }
    
    
} // namespace iff_pvt



class IffInput : public ImageInput {
public:
    IffInput () { init(); }
    virtual ~IffInput () { }
    virtual const char *format_name (void) const { return "iff"; }
    virtual bool open (const std::string &name, ImageSpec &spec);
    virtual bool close (void);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool read_native_tile (int x, int y, int z, void *data);
private:
    FILE *m_fd;
    std::string m_filename;
    iff_pvt::IffFileHeader m_iff_header;
    std::vector<uint8_t> m_buf;
    
    uint32_t m_tbmp_start;
    
    // init to initialize state
    void init (void) {
        m_fd = NULL;
        m_filename.clear ();
        m_buf.clear();
    }
  
    // helper to read an image
    bool readimg (void);
    
    // helper to uncompress a rle channel
    size_t uncompress_rle_channel(
        const uint8_t * in, uint8_t * out, int size
    );
};



class IffOutput : public ImageOutput {
public:
    IffOutput () { init (); }
    virtual ~IffOutput () { close (); }
    virtual const char *format_name (void) const { return "iff"; }
    virtual bool supports (const std::string &feature) const;
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode);
    virtual bool close (void);
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool write_tile (int x, int y, int z,
                             TypeDesc format, const void *data,
                             stride_t xstride, stride_t ystride, stride_t zstride);
private:
    FILE *m_fd;
    std::string m_filename;
    iff_pvt::IffFileHeader m_iff_header;
    std::vector<uint8_t> m_buf;
    
    void init (void) {
        m_fd = NULL;
        m_filename.clear ();
    }
    
    // helper to compress verbatim
    void compress_verbatim (
        const uint8_t *& in, uint8_t *& out, int size
    );
      
    // helper to compress duplicate
    void compress_duplicate (
        const uint8_t *&in, uint8_t *& out, int size
    );
      
    // helper to compress a rle channel
    size_t compress_rle_channel (
        const uint8_t *in, uint8_t *out, int size
    );
};

OIIO_PLUGIN_NAMESPACE_END

#endif // OPENIMAGEIO_IFF_H
