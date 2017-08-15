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
#ifndef OPENIMAGEIO_BMP_H
#define OPENIMAGEIO_BMP_H

#include <cstdio>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>

OIIO_PLUGIN_NAMESPACE_BEGIN


namespace bmp_pvt {

    // size of the BMP file header (the first header that occur in BMP file)
    const int BMP_HEADER_SIZE = 14;

    // sizes of various DIB haders
    const int OS2_V1 = 12;
    const int WINDOWS_V3 = 40;
    const int WINDOWS_V4 = 108;
    const int WINDOWS_V5 = 124;

    // bmp magic numbers
    const int16_t MAGIC_BM = 0x4D42;
    const int16_t MAGIC_BA = 0x4142;
    const int16_t MAGIC_CI = 0x4943;
    const int16_t MAGIC_CP = 0x5043;
    const int16_t MAGIC_PT = 0x5450;

    //const int32_t RLE4_COMPRESSION = 2;
    
    // store informations about BMP file
    class BmpFileHeader {
     public:
         // reads informations about BMP file
         bool read_header (FILE *fd);

         // writes information about bmp file to given file
         bool write_header (FILE *fd);

         // return true if given file is BMP file
         bool isBmp () const;

         int16_t magic; // used to identify BMP file
         int32_t fsize; // size of the BMP file
         int16_t res1;  // reserved
         int16_t res2;  // reserved
         int32_t offset; //offset of image data
     private:
         void swap_endian (void);
    };

    // stores information about bitmap
    class DibInformationHeader {
     public:
         // reads informations about bitmap
         bool read_header (FILE *fd);

         // writes informations about bitmap
         bool write_header (FILE *fd);

         int32_t size; // size of the header
         int32_t width; // bitmap width in pixels
         int32_t height; // bitmap height in pixels
         int16_t cplanes; // number of color planes - always 1
         int16_t bpp; // number of bits per pixel, image color depth
         int32_t compression; // compression used in file
         int32_t isize; // size of the raw image data
         int32_t hres; // horizontal resolution in pixels per meter
         int32_t vres; // vertical resolutions in pixels per meter
         int32_t cpalete; // number of entries in the color palete
         int32_t important; // number of importatn color used, 
                            // 0 - all colors are important,
                            // in most cases ignored

         // added in Version 4 of the format
         int32_t red_mask;
         int32_t blue_mask;
         int32_t green_mask;
         int32_t alpha_mask;
         int32_t cs_type;    //color space type
         int32_t red_x;
         int32_t red_y;
         int32_t red_z;
         int32_t green_x;
         int32_t green_y;
         int32_t green_z;
         int32_t blue_x;
         int32_t blue_y;
         int32_t blue_z;
         int32_t gamma_x;
         int32_t gamma_y;
         int32_t gamma_z;

         // added in Version 5 of the format
         int32_t intent;
         int32_t profile_data;
         int32_t profile_size;
         int32_t reserved;

     private:
         void swap_endian (void);
    };

    struct color_table {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t unused;
    };
} //namespace bmp_pvt



class BmpInput final : public ImageInput {
 public:
    BmpInput () { init (); }
    virtual ~BmpInput () { close (); }
    virtual const char *format_name (void) const { return "bmp"; }
    virtual bool valid_file (const std::string &filename) const;
    virtual bool open (const std::string &name, ImageSpec &spec);
    virtual bool close (void);
    virtual bool read_native_scanline (int y, int z, void *data);
 private:
    int m_padded_scanline_size;
    int m_pad_size;
    FILE *m_fd;
    bmp_pvt::BmpFileHeader m_bmp_header;
    bmp_pvt::DibInformationHeader m_dib_header;
    std::string m_filename;
    std::vector<bmp_pvt::color_table> m_colortable;
    fpos_t m_image_start;
    void init (void) {
        m_padded_scanline_size = 0;
        m_pad_size = 0;
        m_fd = NULL;
        m_filename.clear ();
        m_colortable.clear ();
    }

    bool read_color_table (void);
};



class BmpOutput final : public ImageOutput {
 public:
    BmpOutput () { init (); }
    virtual ~BmpOutput () { close (); }
    virtual const char *format_name (void) const { return "bmp"; }
    virtual int supports (string_view feature) const;
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode);
    virtual bool close (void);
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool write_tile (int x, int y, int z, TypeDesc format,
                             const void *data, stride_t xstride,
                             stride_t ystride, stride_t zstride);
 private:
    int m_padded_scanline_size;
    FILE *m_fd;
    std::string m_filename;
    bmp_pvt::BmpFileHeader m_bmp_header;
    bmp_pvt::DibInformationHeader m_dib_header;
    fpos_t m_image_start;
    unsigned int m_dither;
    std::vector<unsigned char> m_tilebuffer;

    void init (void) {
        m_padded_scanline_size = 0;
        m_fd = NULL;
        m_filename.clear ();
    }

    void create_and_write_file_header (void);

    void create_and_write_bitmap_header (void);
};


OIIO_PLUGIN_NAMESPACE_END


#endif // OPENIMAGEIO_BMP_H
