/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
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


//////////////////////////////////////////////////////////////////////////////
/// \BMP file format
/// 
/// The BMP file format (bitmap or DIB file format) is an image file format
/// used to store bitmap digital images, especially on Microsoft Windows and
/// OS/2. A typical BMP file usually contains the following blocks of data:
///
/// BMP File Header - Stores general informtion about the BMP file.
/// Bitmap Information (DIB header) - Stores detailed information about the
///                                   bitmap image.
/// Color Palette   - Stores definition of the colors being used for indexed
///                   color bitmaps.
/// Bitmap Data     - Stores the actual image, pixel by pixel.
///
/// For more information on BMP file format, please visit:
/// http://www.wikipedia.org/wiki/BMP_format
///
//////////////////////////////////////////////////////////////////////////////


#ifndef OPENIMAGEIO_BMP_H
#define OPENIMAGEIO_BMP_H

#include <cstdio>
#include <iostream>
#include <vector>


namespace bmp_pvt {

/// This block of data identify BMP file header
///   type       -   magic number of BMP file.
///                  Allowed value: 0x424D, 0x4241, 0x4349, 0x4350, 
///                                 0x4943 and 0x5054
///   size       -   size of the file in bytes
///   reserved1  -   reserved
///   reserved2  -   reserved
///   offset     -   starting addres of the bitmap data
///
struct BmpHeader 
{
    short type;
    int size;
    short reserved1;
    short reserved2;
    int offset;
};



/// structure that store informations about colors used in bitmaps
/// this structure is used with pictures that have less then 16 bit color depth
/// it is placed directly after BMPHeader and DibHeader
///
struct ColorTable {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char unused;
};



/// Abstract class that represent the DibHeader - it contains detailed
/// information about bitmap.
/// DIB stands for device-independent bitmap.
/// The header has many variations - we can determine which variation is
/// used by examining the size field of the DIB header (first four bytes)
///
class DibHeader {
 public:
    /// create and return subclass than can read data stored in any Dib header
    /// or return NULL if found unsuported/corrupted header
    ///
    static DibHeader* return_dib_header (FILE *source);

    DibHeader () { }
    virtual ~DibHeader() { }

    int size;        ///< the size of the header
    int width;       ///< width of the bitmap in pixels
    int height;      ///< heigh of the bitmap in pixels
    short planes;    ///< number of color planes - ALWAYS 1
    short bpp;       ///< number of bits per pixel (color depth)
    int compression; ///< information about compresion method being used
    int raw_size;    ///< raw data size
    int hres;        ///< horizontal resolution - pixel per meter
    int vres;        ///< vertical resolution - pixel per meter
    int colors;      ///< number of colors in the palette
    int important;   ///< number of important colors

    /// read the information from given header 
    /// return true if method read as many bytes as given header have
    ///
    virtual bool read_header (void) = 0;

 protected:
    FILE *m_source;

    void init (void) 
    {
       size = 0;
       width = height = 0;
       planes = bpp = colors = 0;
       compression = 0;
       raw_size = 0;
       hres = vres = 0;
       important = 0;
    }
};



/// this is the class that represents the V3Windows Dib Header
///
class V3Windows : public DibHeader {
 public:
    V3Windows () { }
    V3Windows (FILE *source) {  init(); m_source = source;}
    virtual ~V3Windows () { m_source = NULL; }
    virtual bool read_header (void);
};



/// this header has only five fields - size, width, height, planes, bmp
///
class V1Os2 : public DibHeader {
 public:
   V1Os2 () { }
   V1Os2 (FILE *source) { init(); m_source = source; }
   virtual ~V1Os2 () { m_source = NULL; }
   virtual bool read_header (void);
};


}; /* end namespace bmp_pvt */ 

#endif // OPENIMAGEIO_BMP_H
