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
#include "bmp_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace bmp_pvt;

// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

    DLLEXPORT int bmp_imageio_version = OIIO_PLUGIN_VERSION;
    DLLEXPORT ImageInput *bmp_input_imageio_create () {
        return new BmpInput;
    }
    DLLEXPORT const char *bmp_input_extensions[] = {
        "bmp", NULL
    };

OIIO_PLUGIN_EXPORTS_END


bool
BmpInput::valid_file (const std::string &filename) const
{
    FILE *fd = fopen (filename.c_str(), "rb");
    if (!fd)
        return false;
    bmp_pvt::BmpFileHeader bmp_header;
    bool ok = bmp_header.read_header(fd) && bmp_header.isBmp();
    fclose (fd);
    return ok;
}



bool
BmpInput::open (const std::string &name, ImageSpec &spec)
{
    // saving 'name' for later use
    m_filename = name;

    m_fd = fopen (m_filename.c_str (), "rb");
    if (!m_fd) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    // we read header of the file that we think is BMP file
    if (! m_bmp_header.read_header (m_fd)) {
        error ("\"%s\": wrong bmp header size", m_filename.c_str());
        close ();
        return false;
    }
    if (! m_bmp_header.isBmp ()) {
        error ("\"%s\" is not a BMP file, magic number doesn't match",
               m_filename.c_str());
        close ();
        return false;
    }
    if (! m_dib_header.read_header (m_fd)) {
        error ("\"%s\": wrong bitmap header size", m_filename.c_str());
        close ();
        return false;
    }

    const int nchannels = (m_dib_header.bpp == 32) ? 4 : 3;
    const int height = (m_dib_header.height >= 0) ? m_dib_header.height
                                                  : -m_dib_header.height;
    m_spec = ImageSpec (m_dib_header.width, height, nchannels, TypeDesc::UINT8);
    m_spec.attribute ("oiio:BitsPerSample", (int)m_dib_header.bpp);
    m_spec.attribute ("XResolution", (int)m_dib_header.hres);
    m_spec.attribute ("YResolution", (int)m_dib_header.vres);
    m_spec.attribute ("ResolutionUnit", "m");

    // comupting size of one scanline - this is the size of one scanline that
    // is stored in the file, not in the memory
    int swidth = 0;
    switch (m_dib_header.bpp) {
        case 32 :
        case 24 :
            m_scanline_size = ((m_spec.width * m_spec.nchannels) + 3) & ~3;
            break;
        case 16 :
            m_scanline_size = ((m_spec.width << 1) + 3) & ~3;
            break;
        case  8 :
            m_scanline_size = (m_spec.width + 3) & ~3;
            if (! read_color_table ())
                return false;
            break;
        case 4 :
            swidth = (m_spec.width + 1) / 2;
            m_scanline_size = (swidth + 3) & ~3;
            if (! read_color_table ())
                return false;
            break;
        case 1 :
            swidth = (m_spec.width + 7) / 8;
            m_scanline_size = (swidth + 3) & ~3;
            if (! read_color_table ())
                return false;
            break;
    }

    // file pointer is set to the beginning of image data
    // we save this position - it will be helpfull in read_native_scanline
    fgetpos (m_fd, &m_image_start);

    spec = m_spec;
    return true;
}



bool
BmpInput::read_native_scanline (int y, int z, void *data)
{
    if (y < 0 || y > m_spec.height)
        return false;

    // if the height is positive scanlines are stored bottom-up
    if (m_dib_header.width >= 0)
        y = m_spec.height - y - 1;
    const int scanline_off = y * m_scanline_size;

    std::vector<unsigned char> fscanline (m_scanline_size);
    fsetpos (m_fd, &m_image_start);
    fseek (m_fd, scanline_off, SEEK_CUR);
    size_t n = fread (&fscanline[0], 1, m_scanline_size, m_fd);
    if (n != (size_t)m_scanline_size) {
        if (feof (m_fd))
            error ("Hit end of file unexpectedly");
        else
            error ("read error");
        return false;   // Read failed
    }

    // in each case we process only first m_spec.scanline_bytes () bytes
    // as only they contain information about pixels. The rest are just
    // because scanline size have to be 32-bit boundary
    if (m_dib_header.bpp == 24 || m_dib_header.bpp == 32) {
        for (unsigned int i = 0; i < m_spec.scanline_bytes (); i += m_spec.nchannels)
            std::swap (fscanline[i], fscanline[i+2]);
        memcpy (data, &fscanline[0], m_spec.scanline_bytes());
        return true;
    }

    std::vector<unsigned char> mscanline (m_spec.scanline_bytes());
    if (m_dib_header.bpp == 16) {
        const uint16_t RED = 0x7C00;
        const uint16_t GREEN = 0x03E0;
        const uint16_t BLUE = 0x001F;
        for (unsigned int i = 0, j = 0; j < m_spec.scanline_bytes(); i+=2, j+=3) {
            uint16_t pixel = (uint16_t)*(&fscanline[i]);
            mscanline[j] = (uint8_t)((pixel & RED) >> 8);
            mscanline[j+1] = (uint8_t)((pixel & GREEN) >> 4);
            mscanline[j+2] = (uint8_t)(pixel & BLUE);
        }
    }
    if (m_dib_header.bpp == 8) {
        for (unsigned int i = 0, j = 0; j < m_spec.scanline_bytes(); ++i, j+=3) {
            mscanline[j] = m_colortable[fscanline[i]].r;
            mscanline[j+1] = m_colortable[fscanline[i]].g;
            mscanline[j+2] = m_colortable[fscanline[i]].b;
        }
    }
    if (m_dib_header.bpp == 4) {
        for (unsigned int i = 0, j = 0; j + 6 < m_spec.scanline_bytes(); ++i, j+=6) {
            uint8_t mask = 0xF0;
            mscanline[j] = m_colortable[(fscanline[i] & mask) >> 4].r;
            mscanline[j+1] = m_colortable[(fscanline[i] & mask) >> 4].g;
            mscanline[j+2] = m_colortable[(fscanline[i] & mask) >> 4].b;
            mask = 0x0F;
            mscanline[j+3] = m_colortable[fscanline[i] & mask].r;
            mscanline[j+4] = m_colortable[fscanline[i] & mask].g;
            mscanline[j+5] = m_colortable[fscanline[i] & mask].b;
        }
    }
    if (m_dib_header.bpp == 1) {
        for (unsigned int i = 0, k = 0; i < fscanline.size (); ++i) {
            for (int j = 7; j >= 0; --j, k+=3) {
                if (k + 2 >= mscanline.size())
                    break;
                int index = 0;
                if (fscanline[i] & (1 << j))
                    index = 1;
                mscanline[k] = m_colortable[index].r;
                mscanline[k+1] = m_colortable[index].g;
                mscanline[k+2] = m_colortable[index].b;
            }
        }
    }
    memcpy (data, &mscanline[0], m_spec.scanline_bytes());
    return true;
}



bool inline
BmpInput::close (void)
{
    if (m_fd) {
        fclose (m_fd);
        m_fd = NULL;
    }
    init ();
    return true;
}



bool
BmpInput::read_color_table (void)
{
    // size of color table is defined  by m_dib_header.cpalete
    // if this field is 0 - color table has max colors:
    // pow(2, m_dib_header.cpalete) otherwise color table have
    // m_dib_header.cpalete entries
    const int32_t colors = (m_dib_header.cpalete) ? m_dib_header.cpalete :
                                                    1 << m_dib_header.bpp;
    size_t entry_size = 4;
    // if the file is OS V2 bitmap color table entr has only 3 bytes, not four
    if (m_dib_header.size == OS2_V1)
        entry_size = 3;
    m_colortable.resize (colors);
    for (int i = 0; i < colors; i++) {
        size_t n = fread (&m_colortable[i], 1, entry_size, m_fd);
        if (n != entry_size) {
            if (feof (m_fd))
                error ("Hit end of file unexpectedly while reading color table");
            else
                error ("read error while reading color table");
            return false;   // Read failed
        }
    }
    return true;  // ok
}

OIIO_PLUGIN_NAMESPACE_END

