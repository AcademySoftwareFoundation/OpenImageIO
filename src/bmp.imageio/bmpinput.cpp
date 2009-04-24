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

#include <cassert>
#include <cstdio>

#include "imageio.h"
using namespace OpenImageIO;

#include "bmp.h"
using namespace bmp_pvt;



class BmpInput : public ImageInput {
  public:
    BmpInput () { init(); }
    virtual ~BmpInput () { close(); }
    virtual const char * format_name (void) const { return "bmp"; }
    virtual bool open (const std::string &name, ImageSpec &spec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool close ();

  private:
    FILE *m_fd;                       ///< input file handler
    std::string m_filename;           ///< input file name
    BmpHeader m_hbmp;                 ///< BMP Header
    DibHeader *m_dbmp;                ///< BMP DIB Header
    std::vector<unsigned char> m_buf; ///< Buffer the image pixels

    /// Read the entire image and store it in m_buf.
    /// Return true if method read as many bytes as the image contains.
    bool read_bmp_image(void);

    /// Check the file header.
    /// Return true if the file is bmp file.
    bool check_bmp_header (void);

    /// Read the file header.
    /// Return true if method read as many bytes as is needed by the header.
    bool read_bmp_header (void);

    /// Read the dib header.
    /// Return true if method read as many byte as is needed by the header.
    bool read_dib_header (void);

    /// read images with 4-bit color depth
    ///
    bool read_bmp_image4 (void);

    /// read images with 8-bit color depth
    ///
    bool read_bmp_image8 (void);

    /// read images with 24- and 32-bit colors depths
    ///
    bool read_bmp_imageRGB (void);

    /// read color table from file
    /// return pointer to created ColorTable struct
    ///
    ColorTable* read_color_table (void);

    void init () 
    {
      m_fd = NULL;
      m_dbmp = NULL;
      m_buf.clear();
      m_filename.clear();
    }
};



extern "C" {
    DLLEXPORT int bmp_imageio_version = OPENIMAGEIO_PLUGIN_VERSION;
    DLLEXPORT ImageInput *bmp_input_imageio_create () {
        return new BmpInput;
    }
    DLLEXPORT const char *bmp_input_extensions[] = {
        "bmp", NULL
    };
};



bool
BmpInput::open (const std::string &name, ImageSpec &newspec) 
{
    m_filename = name;
    // check if we can open the file
    m_fd = fopen (name.c_str(), "rb");
    if (m_fd == NULL) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    // check if file is bmp file
    if ( ! check_bmp_header()) {
        fclose (m_fd);
        m_fd = NULL;
        error ("\"%s\" is a BMP file, magic number doesn't match", name.c_str());
        return false;
    }

    // read meta-data
    read_bmp_header();
    read_dib_header();
    // BMP files have at least 3 channels
    int nchannels = (m_dbmp->bpp == 32) ? 4 : 3;
    // BMP files can have negative height!
    const int height = (m_dbmp->height >= 0) ? m_dbmp->height : -m_dbmp->height;
    m_spec = ImageSpec (m_dbmp->width, height, nchannels, TypeDesc::UINT8);
    newspec = m_spec;
    return true;
}



bool
BmpInput::close (void) 
{
   if (m_fd != NULL)
      fclose(m_fd);
   delete m_dbmp;
   init ();    //reset to initial state
   return true;
}



bool
BmpInput::read_native_scanline (int y, int z, void *data) 
{ 
    if (m_buf.empty())
        read_bmp_image();

    if (y < 0 || y >= m_spec.height)
        return false;

    // if height of the bitmap is negative number BMP data is stored top-down
    // if height of the bitmap is positive number BMP data is stored bottom-up
    // so for the line y=0 we need to get the line height-y from the disk(buffer)
    const int bufline = (m_dbmp->height < 0) ? y : m_spec.height - 1 - y;
    const int scanline_size = m_spec.width * m_spec.nchannels;
    memcpy(data, &m_buf[bufline * scanline_size], scanline_size);
    return true;
}



bool
BmpInput::check_bmp_header (void) 
{
    // magic numbers of bmp file format
    const short MAGIC1 = 0x424D, MAGIC1_OTHER_ENDIAN = 0x4D42;
    short magic = 0;
    fread (&magic, 2, 1, m_fd);
    rewind (m_fd);
    if (magic != MAGIC1 && magic != MAGIC1_OTHER_ENDIAN)
        return false;
    return true;
}



bool
BmpInput::read_bmp_header (void)
{
    size_t bytes = 0;
    bytes += fread (&m_hbmp.type, 1, 2, m_fd);
    bytes += fread (&m_hbmp.size, 1, 4, m_fd);
    bytes += fread (&m_hbmp.reserved1, 1, 2, m_fd);
    bytes += fread (&m_hbmp.reserved2, 1, 2, m_fd);
    bytes += fread (&m_hbmp.offset, 1, 4, m_fd);

    return (bytes == 14);
}



bool
BmpInput::read_dib_header (void) 
{
    m_dbmp = DibHeader::return_dib_header(m_fd);
    // if size of the header doesn't match the size of supported headers
    // either we found unsuported header or it is corrupted
    if (m_dbmp == NULL)
        return false;

    m_dbmp->read_header();
    return true;
}



bool
BmpInput::read_bmp_image(void) 
{
    const int buf_size = m_dbmp->height * m_dbmp->width * m_spec.nchannels;
    m_buf.resize (buf_size);

    if (m_dbmp->bpp == 4)
        return read_bmp_image4();
    if (m_dbmp->bpp == 8)
        return read_bmp_image8();
    if (m_dbmp->bpp == 24 || m_dbmp->bpp == 32)
        return read_bmp_imageRGB();
    return false;
}



bool
BmpInput::read_bmp_image4 (void)
{
    ColorTable *colors_table = read_color_table ();
    // each byte store info about two pixels
    const int width = (m_spec.width % 2) ? (m_spec.width >> 1) + 1
                                : m_spec.width >> 1;
    const int pad_size = (width % 4) ? 4 - (width % 4) : 0;
    const int scanline_size = width + pad_size;
    std::vector<unsigned char> scanline (scanline_size);
    for (int i = 0; i < m_spec.height; ++i) {
        fread (&scanline[0], 1, scanline_size, m_fd);
        ColorTable *current_ct = NULL;
        const int current_scanline_off = i * m_spec.width * m_spec.nchannels;
        for (int j = 0; j < width - 1; ++j) {
            // choosing apprioprate color table and then converting
            // color index to RGB value;
            current_ct = &colors_table[(scanline[j] & 0xF0) >> 4];
            m_buf[current_scanline_off + j* 6] = current_ct->red;
            m_buf[current_scanline_off + j* 6 + 1] = current_ct->green;
            m_buf[current_scanline_off + j* 6 + 2] = current_ct->blue;
            // second pixel in this byte
            current_ct = &colors_table[scanline[j] & 0x0F];
            m_buf[current_scanline_off + j* 6 + 3] = current_ct->red;
            m_buf[current_scanline_off + j* 6 + 4] = current_ct->green;
            m_buf[current_scanline_off + j* 6 + 5] = current_ct->blue;
        }
        // last two pixels
        const int off = current_scanline_off + (width - 1) * 6;
        current_ct = &colors_table[(scanline[width-1] & 0xF0) >> 4]; 
        m_buf[off] = current_ct->red;
        m_buf[off + 1] = current_ct->green;
        m_buf[off + 2] = current_ct->blue;
        if ( ! (m_spec.width % 2)){
           current_ct = &colors_table[scanline[width-1] & 0x0F]; 
           m_buf[off + 3] = current_ct->red;
           m_buf[off + 4] = current_ct->green;
           m_buf[off + 5] = current_ct->blue;
        }
    }
    delete colors_table;
    return true;
}



bool
BmpInput::read_bmp_image8 (void)
{
    ColorTable *colors_table = read_color_table ();
    //scanlines are padded to 4 byte boundary
    const int pad_size = (m_spec.width % 4) ? 4 - (m_spec.width % 4) : 0;
    // each byte store info about one pixels
    const int scanline_size = m_spec.width + pad_size;
    std::vector<unsigned char> scanline (scanline_size);
    ColorTable *current_ct = NULL;
    for (int i = 0; i < m_spec.height; ++i) {
        fread (&scanline[0], 1, scanline_size, m_fd);
        // where to save current scanline in m_buf
        const int current_scanline_off = i * m_spec.width * m_spec.nchannels;
        for (int j = 0; j < m_spec.width; ++j) {
            // choosing apprioprate color table and then converting
            // color index to RGB value;
            current_ct = &colors_table[scanline[j]];
            m_buf[current_scanline_off + j * 3] = current_ct->red;
            m_buf[current_scanline_off + j * 3 + 1] = current_ct->green;
            m_buf[current_scanline_off + j * 3 + 2] = current_ct->blue;
        }
    }
    delete colors_table;
    return true;
}



bool
BmpInput::read_bmp_imageRGB (void)
{
    if (fseek(m_fd, m_hbmp.offset , SEEK_SET))
        return false;

    // scanlines are padded to 4 byte boudary
    const int pad_size = (m_spec.width % 4) ? 4 - (m_spec.width % 4) : 0;
    const int scanline_size = (m_spec.width + pad_size) * m_spec.nchannels;
    std::vector<unsigned char> scanline (scanline_size);
    //width of the image without padding
    const int width = m_spec.width * m_spec.nchannels;
    for (int i = 0; i < m_spec.height; ++i) {
        fread (&scanline[0], 1, scanline_size, m_fd);
        // In the disk, the pixel values are in BGR format (3 channels)
        // or in BGRA (4 channels), so we need to convert to RGB or RGBA
        const int current_scanline = i * width;
        for (int j = 0; j < width; j += m_spec.nchannels) {
            m_buf[current_scanline + j] = scanline[j+2];
            m_buf[current_scanline + j + 1] = scanline[j+1];
            m_buf[current_scanline + j + 2] = scanline[j];

            // move this after 'for' loop ?
            if (m_spec.nchannels == 4)
                m_buf[current_scanline + j + 4] = scanline[j+4];
        }
    }
    return true;
}



ColorTable*
BmpInput::read_color_table (void)
{
    // size of color table is defined  by m_dbmp->colors
    // if this field is 0 - color table has max colors: pow(2, m_dbmp->bpp)
    // otherwise color table have m_dbmp->colors entries
    const int num_of_colors = (! m_dbmp->colors) ? (1 << m_dbmp->bpp)
                               : m_dbmp->colors;
    ColorTable *colors_table = new ColorTable[num_of_colors];

    // color table is stored directly after BMP File header
    // and DIB header
    const int color_table_offset = 14 + m_dbmp->size;
    fseek (m_fd, color_table_offset, SEEK_SET);
    fread (&colors_table[0], sizeof(ColorTable), num_of_colors, m_fd);

    return colors_table;
}
