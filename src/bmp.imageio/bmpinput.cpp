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
    m_spec = ImageSpec (m_dbmp->width, m_dbmp->height, nchannels, TypeDesc::UINT8);
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

    // The BMP file is flipped vertically in the disk, 
    // so for the line y=0 I need to get the line height-y from the disk(buffer)
    const int bufline = m_spec.height - 1 - y;
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
    fseek(m_fd, m_hbmp.offset , SEEK_SET);
    const int buf_size = m_dbmp->height * m_dbmp->width * m_spec.nchannels;
    m_buf.resize (buf_size);

    // Assuming pixel size of 24- (RGB) or 32-bits (RGBA)
    fread(&m_buf[0], 1, buf_size, m_fd);

    // In the disk, the pixel values are in BGR format (3 channels)
    // or in BGRA (4 channels), so we need to convert to RGB or RGBA
    for (int j=0; j < buf_size; j+=m_spec.nchannels){
        unsigned char aux = m_buf[j];
        m_buf[j] = m_buf[j+2];
        m_buf[j+2] = aux;
        // if m_spec.nchannels==4 the alpha channel is in good place
    }

    return true;
}
