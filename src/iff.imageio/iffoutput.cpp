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

#include "iff_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace iff_pvt;


// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

    DLLEXPORT ImageOutput *iff_output_imageio_create () {
        return new IffOutput;
    }
    DLLEXPORT const char *iff_output_extensions[] = {
        "iff", "z", NULL
    };

OIIO_PLUGIN_EXPORTS_END



bool
IffOutput::supports (const std::string &feature) const
{
    if (feature == "tiles")
        return true;

    // Everything else, we either don't support or don't know about
    return false;
}



bool 
IffOutput::open (const std::string &name, const ImageSpec &spec,
                 OpenMode mode)
{
    // Autodesk Maya documentation:
    // "Maya Image File Format - IFF
    // 
    // Maya supports images in the Interchange File Format (IFF).
    // IFF is a generic structured file access mechanism, and is not only
    // limited to images.
    //
    // The openimageio IFF implementation deals specifically with Maya IFF
    // images with it's data blocks structured as follows:
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
      
    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    close ();  // Close any already-opened file
    // saving 'name' and 'spec' for later use
    m_filename = name;
    m_spec = spec;

    // tiles
    m_spec.tile_width = tile_width();
    m_spec.tile_height = tile_height();
    
    m_fd = fopen (m_filename.c_str (), "wb");
    if (!m_fd) {
        error ("Unable to open file \"%s\"", m_filename.c_str ());
        return false;
    }

    // IFF image files only supports UINT8 and UINT16.  If something
    // else was requested, revert to the one most likely to be readable
    // by any IFF reader: UINT8
    if (m_spec.format != TypeDesc::UINT8 && m_spec.format != TypeDesc::UINT16) 
        m_spec.set_format (TypeDesc::UINT8);

    // check if the client wants the image to be run length encoded
    // currently only RGB RLE compression is supported, we default to RLE
    // as Maya does not handle non-compressed IFF's very well.
    m_iff_header.compression =
    (m_spec.get_string_attribute ("compression", "none") == std::string("none")) ? NONE : RLE;

    // we write the header of the file
    m_iff_header.x = m_spec.x;
    m_iff_header.y = m_spec.y;
    m_iff_header.width = m_spec.width;
    m_iff_header.height = m_spec.height;
    m_iff_header.tiles = tile_width_size (m_spec.width) * tile_height_size (m_spec.height);
    m_iff_header.pixel_bits = m_spec.format == TypeDesc::UINT8 ? 8 : 16;
    m_iff_header.pixel_channels = m_spec.nchannels;
    m_iff_header.author = m_spec.get_string_attribute ("Artist");
    m_iff_header.date = m_spec.get_string_attribute ("DateTime");
    
    if (!m_iff_header.write_header (m_fd)) {
        error ("\"%s\": could not write iff header", m_filename.c_str ());
        close ();
        return false;
    }

    return true;
}



bool 
IffOutput::write_scanline (int y, int z, TypeDesc format, const void *data,
                           stride_t xstride)
{
    // scanline not used for Maya IFF, uses tiles instead.
    return false;
}



bool 
IffOutput::write_tile (int x, int y, int z,
                       TypeDesc format, const void *data,
                       stride_t xstride, stride_t ystride, stride_t zstride)
{
    if (m_buf.empty ())
        // resize buffer
        m_buf.resize (m_spec.image_bytes());
  
    // auto stride
    m_spec.auto_stride (xstride, ystride, zstride, format, spec().nchannels,
                        spec().tile_width, spec().tile_height);
    
    // native tile
    std::vector<uint8_t> scratch;    
    data = to_native_tile (format, data, xstride, ystride, zstride, scratch);   
                        
    x -= m_spec.x;   // Account for offset, so x,y are file relative, not 
    y -= m_spec.y;   // image relative  
  
    // tile size
    int w = m_spec.width;
    int tw = std::min (x + m_spec.tile_width, m_spec.width) - x;
    int th = std::min (y + m_spec.tile_height, m_spec.height) - y;
    
    // tile data
    int iy=0;
    for(int oy = y; oy < y + th; oy++) {
        // in
        uint8_t *in_p = (uint8_t*)data + 
                        (iy * m_spec.tile_width) * 
                        m_spec.pixel_bytes();
        // out
        uint8_t *out_p = &m_buf[0] + 
                         (oy * w + x) * 
                         m_spec.pixel_bytes(); 
        // copy
        memcpy (out_p, in_p, tw * m_spec.pixel_bytes());
        iy++;
    }

    return true;    
}



inline bool
IffOutput::close (void)
{
    if (m_fd) {
      
        // flip buffer to make write tile easier,
        // from tga.imageio:

        int bytespp = m_spec.pixel_bytes();

        std::vector<unsigned char> flip (m_spec.width * bytespp);
        unsigned char *src, *dst, *tmp = &flip[0];
        for (int y = 0; y < m_spec.height / 2; y++) {
          src = &m_buf[(m_spec.height - y - 1) * m_spec.width * bytespp];
          dst = &m_buf[y * m_spec.width * bytespp];

          memcpy (tmp, src, m_spec.width * bytespp);
          memcpy (src, dst, m_spec.width * bytespp);
          memcpy (dst, tmp, m_spec.width * bytespp);
        }
        
        // write y-tiles
        for(uint32_t ty=0; ty<tile_height_size(m_spec.height); ty++) {
        
            // write x-tiles
            for(uint32_t tx=0; tx<tile_width_size(m_spec.width); tx++) {
            
                // channels
                uint8_t channels = m_iff_header.pixel_channels;            
            
                // set tile coordinates
                uint16_t xmin, xmax, ymin, ymax;
      
                // set xmin and xmax
                xmin = tx * tile_width();
                xmax = std::min (xmin + tile_width(), m_spec.width) - 1;
      
                // set ymin and ymax
                ymin = ty * tile_height();
                ymax = std::min (ymin + tile_height(), m_spec.height) - 1;
                
                // set width and height
                uint32_t tw = xmax - xmin + 1;
                uint32_t th = ymax - ymin + 1;
                  
                // write 'RGBA' type
                std::string tmpstr = "RGBA";
                if (!fwrite (tmpstr.c_str(), tmpstr.length (), 1, m_fd))
                    return false;      
                  
                // length.
                uint32_t length = tw * th * m_spec.pixel_bytes();

                // tile length.
                uint32_t tile_length = length;
              
                // align.
                length = align_size (length, 4);
                  
                // append xmin, xmax, ymin and ymax.
                length += 8;
              
                // tile compression.
                bool tile_compress = 
                (m_spec.get_string_attribute ("compression", "none") == std::string("none")) ? NONE : RLE;
                
                // set bytes.
                std::vector<uint8_t> scratch;
                scratch.resize (tile_length);
              
                uint8_t * out_p = static_cast<uint8_t*>(&scratch[0]);
              
                // handle 8-bit data
                if (m_spec.format == TypeDesc::UINT8) {
                
                    if (tile_compress) {
                    
                        uint32_t index = 0, size = 0;
                        std::vector<uint8_t> tmp;
                    
                        // set bytes.
                        tmp.resize (tile_length * 2);    
                  
                        // map: RGB(A) to BGRA
                        for (int c =(channels * m_spec.channel_bytes()) - 1; c>=0; --c) {
                        
                            std::vector<uint8_t> in (tw * th);
                            uint8_t *in_p = &in[0];
                  
                            // set tile
                            for (uint16_t py=ymin; py<=ymax; py++) {
                            
                                const uint8_t * in_dy = &m_buf[0] + 
                                                        (py * m_spec.width) * 
                                                        m_spec.pixel_bytes();     
                                                                  
                                for (uint16_t px=xmin; px<=xmax; px++) {
                                    // get pixel
                                    uint8_t pixel;
                                    const uint8_t * in_dx = in_dy + px * m_spec.pixel_bytes() + c;
                                    memcpy (&pixel, in_dx, 1);
                                    // set pixel
                                    *in_p++ = pixel;
                                }
                            }
                    
                            // compress rle channel
                            size = compress_rle_channel (&in[0], &tmp[0] + index, tw * th);
                            index += size;
                        }
                  
                        // if size exceeds tile length write uncompressed
                  
                        if (index < tile_length) {
                        
                            memcpy (&scratch[0], &tmp[0], index);
                    
                            // set tile length
                            tile_length = index;

                            // append xmin, xmax, ymin and ymax
                            length = index + 8;

                            // set length
                            uint32_t align = align_size (length, 4);
                            if (align > length) {
                            
                                out_p = &scratch[0] + index;
                                // Pad.
                                for (uint32_t i=0; i<align-length; i++) {
                                    *out_p++ = '\0';
                                    tile_length++;
                                }
                            }
                        }
                        else
                        {
                            tile_compress = false;
                        }
                    }
                    if (!tile_compress) {
                    
                        for (uint16_t py=ymin; py<=ymax; py++) {
                        
                            const uint8_t * in_dy = &m_buf[0] + 
                                                    (py * m_spec.width) * 
                                                    m_spec.pixel_bytes(); 
      
                            for (uint16_t px=xmin; px<=xmax; px++) { 
                            
                                // Map: RGB(A)8 RGBA to BGRA
                                for (int c=channels - 1; c>=0; --c) { 
                                
                                    // get pixel
                                    uint8_t pixel;
                                    const uint8_t * in_dx = in_dy + 
                                                            px * m_spec.pixel_bytes() + 
                                                            c * m_spec.channel_bytes();
                                    memcpy (&pixel, in_dx, 1);           
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
                        tmp.resize (tile_length * 2);      
                  
                        // set map
                        std::vector<uint8_t> map;
                        if (littleendian()) {
                            int rgb16[] = { 0, 2, 4, 1, 3, 5 };
                            int rgba16[] = { 0, 2, 4, 7, 1, 3, 5, 6 };
                            if (m_iff_header.pixel_channels == 3) {
                                map = std::vector<uint8_t>( rgb16, &rgb16[6] );
                            } else {
                                map = std::vector<uint8_t>( rgba16, &rgba16[8] );
                            }
                          
                        } else {
                            int rgb16[] = { 1, 3, 5, 0, 2, 4 };
                            int rgba16[] = { 1, 3, 5, 7, 0, 2, 4, 6 };
                            if (m_iff_header.pixel_channels == 3) {
                                map = std::vector<uint8_t>( rgb16, &rgb16[6] );
                            } else {
                                map = std::vector<uint8_t>( rgba16, &rgba16[8] );
                            }
                        }

                        // map: RRGGBB(AA) to BGR(A)BGR(A)
                        for (int c =(channels * m_spec.channel_bytes()) - 1; c>=0; --c) {
                            int mc = map[c];
                    
                            std::vector<uint8_t> in (tw * th);
                            uint8_t *in_p = &in[0];
                  
                            // set tile
                            for (uint16_t py=ymin; py<=ymax; py++) {
                            
                                const uint8_t * in_dy = &m_buf[0] + 
                                                        (py * m_spec.width) * 
                                                        m_spec.pixel_bytes();
                                                        
                                for (uint16_t px=xmin; px<=xmax; px++) {
                                    // get pixel
                                    uint8_t pixel;
                                    const uint8_t * in_dx = in_dy + px * m_spec.pixel_bytes() + mc;
                                    memcpy (&pixel, in_dx, 1);
                                    // set pixel.
                                    *in_p++ = pixel;
                                }
                            }
                    
                            // compress rle channel
                            size = compress_rle_channel (&in[0], &tmp[0] + index, tw * th);
                            index += size;
                        }
                  
                        // if size exceeds tile length write uncompressed
                      
                        if (index < tile_length) {
                        
                            memcpy (&scratch[0], &tmp[0], index);
                        
                            // set tile length
                            tile_length = index;

                            // append xmin, xmax, ymin and ymax
                            length = index + 8;

                            // set length
                            uint32_t align = align_size (length, 4);
                            if (align > length) {
                            
                                out_p = &scratch[0] + index;
                                // Pad.
                                for (uint32_t i=0; i<align-length; i++) {
                                
                                    *out_p++ = '\0';
                                    tile_length++;
                                }
                            }
                        }
                        else
                        {
                            tile_compress = false;
                        }
                    }
                
                    if (!tile_compress) { 
      
                        for (uint16_t py=ymin; py<=ymax; py++) {

                            const uint8_t * in_dy = &m_buf[0] + 
                                                    (py * m_spec.width) * 
                                                    m_spec.pixel_bytes();
                        
                            for (uint16_t px=xmin; px<=xmax; px++) {
                                // map: RGB(A) to BGRA
                                for (int c=channels - 1; c>=0; --c) { 
                                    uint16_t pixel;
                                    const uint8_t * in_dx = in_dy + 
                                                            px * m_spec.pixel_bytes() + 
                                                            c * m_spec.channel_bytes();
                                    memcpy (&pixel, in_dx, 2);           
                                    if (littleendian()) {
                                        swap_endian (&pixel);
                                    }
                                    // set pixel
                                    *out_p++ = pixel;
                                    (*out_p)++; // avoid gcc4.x warning
                                }
                            }
                        }
                    }
                }
                
                // write 'RGBA' length
                if (littleendian())
                    swap_endian (&length);

                if (!fwrite (&length, sizeof (length), 1, m_fd))
                    return false;  
              
                // write xmin, xmax, ymin and ymax
                if (littleendian()) {
                    swap_endian (&xmin);
                    swap_endian (&ymin);
                    swap_endian (&xmax);
                    swap_endian (&ymax); 
                }
              
                if (!fwrite (&xmin, sizeof (xmin), 1, m_fd) ||
                    !fwrite (&ymin, sizeof (ymin), 1, m_fd) ||
                    !fwrite (&xmax, sizeof (xmax), 1, m_fd) ||
                    !fwrite (&ymax, sizeof (ymax), 1, m_fd))
                return false;   

                // write tile
                if (!fwrite (&scratch[0], tile_length, 1, m_fd))
                    return false;
                
            }
        }
      
        // set sizes
        uint32_t pos, tmppos;
        pos = ftell (m_fd);

        uint32_t p0 = pos - 8;
        uint32_t p1 = p0 - m_iff_header.for4_start;

        // set pos
        tmppos = 4;
        fseek (m_fd, tmppos, SEEK_SET);
    
        // write FOR4 <size> CIMG
        if (littleendian()) {
            swap_endian (&p0);
        }

        if (!fwrite (&p0, sizeof (p0), 1, m_fd))
            return false;      

        // set pos
        tmppos = m_iff_header.for4_start + 4;
        fseek (m_fd, tmppos, SEEK_SET);
    
        // write FOR4 <size> TBMP
        if (littleendian()) {
            swap_endian (&p1);
        }

        if (!fwrite (&p1, sizeof (p1), 1, m_fd))
            return false;     
        
        // close the stream
        fclose (m_fd); 
        m_fd = NULL;
    }
    return true;
}



void
IffOutput::compress_verbatim (
    const uint8_t *& in, uint8_t *& out, int size)
{
    int count = 1;
    unsigned char byte = 0;

    // two in a row or count
    for (; count < size; ++count)
    {
        if (in[count - 1] == in[count]) 
        {
            if (byte == in[count - 1])
            {
                count -= 2;
                break;
            }
        }
        byte = in[count - 1];
    }

    // copy
    *out++ = count - 1;
    memcpy (out, in, count);

    out += count;
    in += count;
}



void
IffOutput::compress_duplicate (
    const uint8_t *& in, uint8_t *& out, int size)
{
    int count = 1;
    for (; count < size; ++count)
    {
        if (in[count - 1] != in[count]) break; 
    }
  
    const bool run = count > 1;
    const int length = run ? 1 : count;
    
    // copy
    *out++ = ((count - 1) & 0x7f) | (run << 7);
    *out = *in;

    out += length;
    in += count;
}



size_t 
IffOutput::compress_rle_channel (
    const uint8_t * in, uint8_t * out, int size)
{
    const uint8_t * const _out = out;
    const uint8_t * const end = in + size;
  
    while (in < end) 
    {
        // find runs
        const int max = std::min (0x7f + 1, static_cast<int>(end - in));
        if (max > 0)
        {
            if (in[0] != in[1])
            {
                // compress verbatim
                compress_verbatim (in, out, max);
            }
            else
            {
                // compress duplicate
                compress_duplicate (in, out, max);
            }
        }
    }
    const size_t r = out - _out;
    return r;
}



OIIO_PLUGIN_NAMESPACE_END

