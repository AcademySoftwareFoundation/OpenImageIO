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
#include "OpenImageIO/dassert.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace iff_pvt;



bool
IffFileHeader::read_header (FILE *fd)
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
    
        // get type
        if (!fread (&type, 1, sizeof (type), fd) ||
            // get length
            !fread (&size, 1, sizeof (size), fd))  
        return false;

        if (littleendian())
            swap_endian (&size);
   
        chunksize = align_size (size, 4);
  
        if (type[0] == 'F' &&
            type[1] == 'O' &&
            type[2] == 'R' &&
            type[3] == '4') {
          
            // get type
            if (!fread (&type, 1, sizeof (type), fd))
                return false;

            // check if CIMG
            if (type[0] == 'C' &&
                type[1] == 'I' &&
                type[2] == 'M' &&
                type[3] == 'G') {
                
                // read TBHD.
                for (;;) {
                    // get type
                    if (!fread (&type, 1, sizeof (type), fd) ||
                        // get length
                        !fread (&size, 1, sizeof (size), fd))
                    return false;    

                    if (littleendian())
                        swap_endian (&size);

                    chunksize = align_size (size, 4);

                    if (type[0] == 'T' &&
                        type[1] == 'B' &&
                        type[2] == 'H' &&
                        type[3] == 'D') {
                        
                        tbhdsize = size;

                        // test if table header size is correct
                        if (tbhdsize!=24 && tbhdsize!=32)
                            return false; // bad table header size

                        // get width and height
                        if (!fread (&width, 1, sizeof (width), fd) ||
                            !fread (&height, 1, sizeof (height), fd) ||
                            
                            // get prnum and prdeb
                            !fread (&prnum, 1, sizeof (prnum), fd) ||
                            !fread (&prden, 1, sizeof (prden), fd) ||
                            
                            // get flags, bytes, tiles and compressed
                            !fread (&flags, 1, sizeof (flags), fd) ||
                            !fread (&bytes, 1, sizeof (bytes), fd) ||
                            !fread (&tiles, 1, sizeof (tiles), fd) ||
                            !fread (&compression, 1, sizeof (compression), fd))
                        return false;

                        // get xy
                        if (tbhdsize == 32) {
                            if (!fread (&x, 1, sizeof (x), fd) ||
                                !fread (&y, 1, sizeof (y), fd))
                            return false;
                        } else {
                            x = 0;
                            y = 0;
                        }
                        
                        // swap endianness
                        if (littleendian()) {
                            swap_endian (&width);
                            swap_endian (&height);
                            swap_endian (&prnum);
                            swap_endian (&prden);
                            swap_endian (&flags);
                            swap_endian (&bytes);
                            swap_endian (&tiles);
                            swap_endian (&compression);
                        }

                        // tiles
                        if (tiles == 0)
                            return false; // non-tiles not supported

                        // 0 no compression
                        // 1 RLE compression
                        // 2 QRL (not supported)
                        // 3 QR4 (not supported)
                        if (compression > 1)
                            return false;

                        // test format.
                        if (flags & RGBA) {
                          
                            // test if black is set
                            DASSERT (!(flags & BLACK));

                            // test for RGB channels.
                            if (flags & RGB) {
                                pixel_channels = 3;
                            }
                          
                            // test for alpha channel
                            if (flags & ALPHA) {
                                pixel_channels++;
                            }
                          
                            // test pixel bits
                            if (!bytes) {
                                pixel_bits = 8; // 8bit
                            } else {
                                pixel_bits = 16; // 16bit
                        }            
                    }

                    // Z format.
                    else if (flags & ZBUFFER) {
                        pixel_channels = 1;
                        pixel_bits = 32; // 32bit
                        // NOTE: Z_F32 support - not supported
                        DASSERT (bytes==0);
                    }
                        
                    // read AUTH, DATE or FOR4
                        
                    for (;;) {

                        // get type
                        if (!fread (&type, 1, sizeof (type), fd) ||
                            // get length
                            !fread (&size, 1, sizeof (size), fd))
                        return false;

                        if (littleendian())
                            swap_endian (&size);

                        chunksize = align_size (size, 4);
                              
                        if (type[0] == 'A' &&
                            type[1] == 'U' &&
                            type[2] == 'T' &&
                            type[3] == 'H') {
                           
                            std::vector<char> str (chunksize);
                            if (!fread (&str[0], 1, str.size(), fd))
                                return false;
                                  
                            // get author
                            author = std::string (&str[0], str.size());
                        } else if (type[0] == 'D' &&
                                   type[1] == 'A' &&
                                   type[2] == 'T' &&
                                   type[3] == 'E') {
                       
                            std::vector<char> str (chunksize);
                            if (!fread (&str[0], 1, str.size(), fd))
                                return false;
                                  
                            // get date
                            date = std::string (&str[0], str.size());                
                        } else if (type[0] == 'F' &&
                                   type[1] == 'O' &&
                                   type[2] == 'R' &&
                                   type[3] == '4') {
                                
                            // get type
                            if (!fread (&type, 1, sizeof (type), fd))
                                return false;

                            // check if CIMG
                            if (type[0] == 'T' &&
                                type[1] == 'B' &&
                                type[2] == 'M' &&
                                type[3] == 'P') {
                                
                                // tbmp position for later user in in 
                                // read_native_tile
                            
                                tbmp_start = ftell (fd);
                                  
                                // read first RGBA block to detect tile size.
                                  
                                for (unsigned int t=0; t<tiles; t++) {
                                    // get type
                                    if (!fread (&type, 1, sizeof (type), fd) ||
                                        // get length
                                        !fread (&size, 1, sizeof (size), fd))
                                    return false;

                                    if (littleendian())
                                        swap_endian (&size);

                                    chunksize = align_size (size, 4);
                                    
                                    // check if RGBA
                                    if (type[0] == 'R' &&
                                        type[1] == 'G' &&
                                        type[2] == 'B' &&
                                        type[3] == 'A') {

                                        // get tile coordinates.
                                        uint16_t xmin, xmax, ymin, ymax;
                                        if (!fread (&xmin, 1, sizeof (xmin), fd) ||
                                            !fread (&ymin, 1, sizeof (ymin), fd) ||
                                            !fread (&xmax, 1, sizeof (xmax), fd) ||
                                            !fread (&ymax, 1, sizeof (ymax), fd))
                                        return false;
                                          
                                        // swap endianness
                                        if (littleendian()) {
                                            swap_endian (&xmin);
                                            swap_endian (&ymin);
                                            swap_endian (&xmax);
                                            swap_endian (&ymax);
                                        }
                                      
                                        // check tile
                                        if (xmin > xmax || 
                                            ymin > ymax || 
                                            xmax >= width || 
                                            ymax >= height) {
                                            return false;
                                        }
                                      
                                        // set tile width and height
                                        tile_width = xmax - xmin + 1;
                                        tile_height = ymax - ymin + 1; 
                 
                                        // done, return
                                        return true;
                                    }
                                    
                                    // skip to the next block.
                                    if (fseek (fd, chunksize, SEEK_CUR))
                                        return false;

                                }
                            } else {
                                // skip to the next block.
                                if (fseek (fd, chunksize, SEEK_CUR))
                                    return false;
                            }
                        } else {
                            // skip to the next block.
                            if (fseek (fd, chunksize, SEEK_CUR))
                                return false;
                        }
                    }
                    // TBHD done, break
                    break;
                }
          
                // skip to the next block.
                if (fseek (fd, chunksize, SEEK_CUR))
                    return false;
            }
        }
    }
    // skip to the next block.
    if (fseek (fd, chunksize, SEEK_CUR))
      return false;
  }
  
  return false;
}



bool
IffFileHeader::write_header (FILE *fd)
{
    uint32_t length = 0;
    uint32_t flags = 0;
    uint16_t prnum = 0;
    uint16_t prden = 0;

    // write 'FOR4' type
    std::string tmpstr = "FOR4";
    if (!fwrite (tmpstr.c_str(), tmpstr.length(), 1, fd))
        return false;

    // write 'FOR4' length, only reserve it for now
    if (littleendian())
        swap_endian (&length);

    if (!fwrite (&length, sizeof (length), 1, fd))
        return false;  

    // write 'CIMG' type
    tmpstr = "CIMG";
    if (!fwrite (tmpstr.c_str(), tmpstr.length(), 1, fd))
        return false;
    
    // write 'TBHD' type
    tmpstr = "TBHD";
    if (!fwrite (tmpstr.c_str(), tmpstr.length(), 1, fd))
        return false;

    // 'TBHD' length, 32 bytes
    length = 32;
    if (littleendian())
        swap_endian (&length);
      
    if (!fwrite (&length, sizeof (length), 1, fd))
        return false;  

    // write width and height
    if (littleendian()) {
        swap_endian (&width);
        swap_endian (&height);
    }

    if (!fwrite (&width, sizeof (width), 1, fd) ||
        !fwrite (&height, sizeof (height), 1, fd))
    return false;    

    // write prnum and prden
    prnum = 1;
    prden = 1;
    if (littleendian()) {
        swap_endian (&prnum);
        swap_endian (&prden);
    }

    if (!fwrite (&prnum, sizeof (prnum), 1, fd) ||
        !fwrite (&prden, sizeof (prden), 1, fd))
    return false;   
    
    // write flags and channels
    if (pixel_channels == 3) {
        flags = RGB;
    } else {
        flags = RGBA; 
    }  

    uint16_t tmpint;
    if (pixel_bits == 8) {
        tmpint = 0;
    } else {
        tmpint = 1;
    }
  
    if (littleendian()) {
        swap_endian (&flags);
        swap_endian (&tmpint);
    }
  
    if (!fwrite (&flags, sizeof (flags), 1, fd) ||
        !fwrite (&tmpint, sizeof (tmpint), 1, fd))
    return false;  

    // write tiles
    if (littleendian()) {
        swap_endian (&tiles);
    }

    if (!fwrite (&tiles, sizeof (tiles), 1, fd))
        return false;  

    // write compression
    // 0 no compression
    // 1 RLE compression
    // 2 QRL (not supported)
    // 3 QR4 (not supported)
    if (littleendian()) {
        swap_endian (&compression);
    }
  
    if (!fwrite (&compression, sizeof (compression), 1, fd))
        return false;  

    // write x and y
    if (littleendian()) {
        swap_endian (&x);
        swap_endian (&y);      
    }
  
    if (!fwrite (&x, sizeof (x), 1, fd) ||
        !fwrite (&y, sizeof (y), 1, fd))
    return false;  
    
    // for4 position for later user in
    // close
            
    for4_start = ftell (fd);

    // write 'FOR4' type
    tmpstr = "FOR4";
    if (!fwrite (tmpstr.c_str(), tmpstr.length(), 1, fd))
        return false;

    // write 'FOR4' length, only reserve it for now
    length = 0;
    if (littleendian())
        swap_endian (&length);

    if (!fwrite (&length, sizeof (length), 1, fd))
        return false;  
  
    // write 'TBMP' type
    tmpstr = "TBMP";
    if (!fwrite (tmpstr.c_str(), tmpstr.length(), 1, fd))
        return false;

    return true;
}



OIIO_PLUGIN_NAMESPACE_END

