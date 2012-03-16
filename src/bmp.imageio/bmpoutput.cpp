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

#include "strutil.h"
#include "bmp_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace bmp_pvt;


// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

    DLLEXPORT ImageOutput *bmp_output_imageio_create () {
        return new BmpOutput;
    }
    DLLEXPORT const char *bmp_output_extensions[] = {
        "bmp", NULL
    };

OIIO_PLUGIN_EXPORTS_END



bool
BmpOutput::open (const std::string &name, const ImageSpec &spec,
                 OpenMode mode)
{
    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    // saving 'name' and 'spec' for later use
    m_filename = name;
    m_spec = spec;

    // TODO: Figure out what to do with nchannels.

    m_fd = fopen (m_filename.c_str (), "wb");
    if (! m_fd) {
        error ("Unable to open file \"%s\"", m_filename.c_str ());
        return false;
    }

    create_and_write_file_header ();
    create_and_write_bitmap_header ();

    // Scanline size is rounded up to align to 4-byte boundary
    m_scanline_size = ((m_spec.width * m_spec.nchannels) + 3) & ~3;
    fgetpos (m_fd, &m_image_start);

    // Only support 8 bit channels for now.
    m_spec.set_format (TypeDesc::UINT8);

    return true;
}



bool
BmpOutput::write_scanline (int y, int z, TypeDesc format, const void *data,
                           stride_t xstride)
{
    if (y > m_spec.height) {
        error ("Attempt to write too many scanlines to %s", m_filename.c_str());
        close ();
        return false;
    }

    if (m_spec.width >= 0)
        y = (m_spec.height - y - 1);
    int scanline_off = y * m_scanline_size;
    fsetpos (m_fd, &m_image_start);
    fseek (m_fd, scanline_off, SEEK_CUR);

    std::vector<unsigned char> scratch;
    data = to_native_scanline (format, data, xstride, scratch);
    std::vector<unsigned char> buf (m_scanline_size);
    memcpy (&buf[0], data, m_scanline_size);

    // Swap RGB pixels into BGR format
    for (int i = 0, iend = buf.size() - 2; i < iend; i += m_spec.nchannels)
        std::swap (buf[i], buf[i+2]);

    fwrite (&buf[0], 1, buf.size (), m_fd);
    return true;
}



bool
BmpOutput::close (void)
{
    if (m_fd) {
        fclose (m_fd);
        m_fd = NULL;
    }
    return true;
}


void
BmpOutput::create_and_write_file_header (void)
{
    m_bmp_header.magic = MAGIC_BM;
    const int data_size = m_spec.width * m_spec.height * m_spec.nchannels;
    const int file_size = data_size + BMP_HEADER_SIZE + WINDOWS_V3;
    m_bmp_header.fsize = file_size;
    m_bmp_header.res1 = 0;
    m_bmp_header.res2 = 0;
    m_bmp_header.offset = BMP_HEADER_SIZE + WINDOWS_V3;

    m_bmp_header.write_header (m_fd);
}



void
BmpOutput::create_and_write_bitmap_header (void)
{
    m_dib_header.size = WINDOWS_V3;
    m_dib_header.width = m_spec.width;
    m_dib_header.height = m_spec.height;
    m_dib_header.cplanes = 1;
    m_dib_header.compression = 0;

    m_dib_header.bpp = m_spec.nchannels << 3;

    m_dib_header.isize = m_spec.width * m_spec.height * m_spec.nchannels; 
    m_dib_header.hres = 0;
    m_dib_header.vres = 0;
    m_dib_header.cpalete = 0;
    m_dib_header.important = 0;

    ImageIOParameter *p = NULL;
    p = m_spec.find_attribute ("ResolutionUnit", TypeDesc::STRING);
    if (p && p->data()) {
        std::string res_units = *(char**)p->data ();
        if (Strutil::iequals (res_units, "m") ||
              Strutil::iequals (res_units, "pixel per meter")) {
            ImageIOParameter *resx = NULL, *resy = NULL;
            resx = m_spec.find_attribute ("XResolution", TypeDesc::INT32);
            if (resx && resx->data())
                m_dib_header.hres = *(int*)resx->data ();
            resy = m_spec.find_attribute ("YResolution", TypeDesc::INT32);
            if (resy && resy->data())
                m_dib_header.vres = *(int*)resy->data ();
        }
    }

    m_dib_header.write_header (m_fd);
}

OIIO_PLUGIN_NAMESPACE_END

