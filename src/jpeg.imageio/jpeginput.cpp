/////////////////////////////////////////////////////////////////////////////
// Copyright 2004 NVIDIA Corporation and Copyright 2008 Larry Gritz.
// All Rights Reserved.
//
// Extensions by Larry Gritz based on open-source code by NVIDIA.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of NVIDIA nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// (This is the Modified BSD License)
/////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <ctype.h>
#include <cstdio>
#include <iostream>

extern "C" {
#include "jpeglib.h"
}

#include "imageio.h"
using namespace OpenImageIO;
#include "fmath.h"
#include "jpeg_pvt.h"



// See JPEG library documentation in /usr/share/doc/libjpeg-devel-6b


class JpgInput : public ImageInput {
 public:
    JpgInput () { init(); }
    virtual ~JpgInput () { close(); }
    virtual const char * format_name (void) const { return "jpeg"; }
    virtual bool open (const char *name, ImageIOFormatSpec &spec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool close ();
 private:
    FILE *fd;
    bool first_scanline;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    void init () { fd = NULL; }
};



// Export version number and create function symbols
extern "C" {
    DLLEXPORT int imageio_version = IMAGEIO_VERSION;
    DLLEXPORT JpgInput *jpeg_input_imageio_create () {
        return new JpgInput;
    }
    DLLEXPORT const char *jpeg_input_extensions[] = {
        "jpg", "jpe", "jpeg", NULL
    };
};



bool
JpgInput::open (const char *name, ImageIOFormatSpec &newspec)
{
    // Check that file exists and can be opened
    fd = fopen (name, "rb");
    if (fd == NULL)
        return false;

    // Check magic number to assure this is a JPEG file
    int magic = 0;
    fread (&magic, 4, 1, fd);
    rewind (fd);
    const int JPEG_MAGIC = 0xffd8ffe0, JPEG_MAGIC_OTHER_ENDIAN =  0xe0ffd8ff;
    const int JPEG_MAGIC2 = 0xffd8ffe1, JPEG_MAGIC2_OTHER_ENDIAN =  0xe1ffd8ff;
    if (magic != JPEG_MAGIC && magic != JPEG_MAGIC_OTHER_ENDIAN &&
        magic != JPEG_MAGIC2 && magic != JPEG_MAGIC2_OTHER_ENDIAN) {
        fclose (fd);
        return false;
    }

    m_spec = ImageIOFormatSpec();

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress (&cinfo);            // initialize decompressor
    jpeg_stdio_src (&cinfo, fd);                // specify the data source

    // Request saving of EXIF and other special tags for later spelunking
    jpeg_save_markers (&cinfo, JPEG_APP0+1, 0xffff);
    // FIXME - also process JPEG_COM marker

    jpeg_read_header (&cinfo, FALSE);           // read the file parameters
    jpeg_start_decompress (&cinfo);             // start working
    first_scanline = true;                      // start decompressor

    m_spec.x = 0;
    m_spec.y = 0;
    m_spec.z = 0;
    m_spec.width = cinfo.output_width;
    m_spec.height = cinfo.output_height;
    m_spec.nchannels = cinfo.output_components;
    m_spec.depth = 1;
    m_spec.full_width = m_spec.width;
    m_spec.full_height = m_spec.height;
    m_spec.full_depth = m_spec.depth;
    m_spec.set_format (PT_UINT8);
    m_spec.tile_width = 0;
    m_spec.tile_height = 0;
    m_spec.tile_depth = 0;

    m_spec.channelnames.clear();
    switch (m_spec.nchannels) {
    case 1:
        m_spec.channelnames.push_back("A");
        break;
    case 3:
        m_spec.channelnames.push_back("R");
        m_spec.channelnames.push_back("G");
        m_spec.channelnames.push_back("B");
        break;
    case 4:
        m_spec.channelnames.push_back("R");
        m_spec.channelnames.push_back("G");
        m_spec.channelnames.push_back("B");
        m_spec.channelnames.push_back("A");
        break;
    default:
        fclose (fd);
        return false;
    }

    for (jpeg_saved_marker_ptr m = cinfo.marker_list;  m;  m = m->next) {
        if (m->marker == (JPEG_APP0+1))
            exif_from_APP1 (m_spec, (unsigned char *)m->data);
    }

    newspec = m_spec;
    return true;
}



bool
JpgInput::read_native_scanline (int y, int z, void *data)
{
    first_scanline = false;
    assert (y == (int)cinfo.output_scanline);
    assert (y < (int)cinfo.output_height);
    jpeg_read_scanlines (&cinfo, (JSAMPLE **)&data, 1); // read one scanline
    return true;
}



bool
JpgInput::close ()
{
    if (fd != NULL) {
        if (!first_scanline)
            jpeg_finish_decompress (&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose (fd);
    }
    init ();   // Reset to initial state
    return true;
}

