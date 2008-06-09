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

#include "imageio.h"
extern "C" {
  #include "jpeglib.h"
}

using namespace OpenImageIO;


// See JPEG library documentation in /usr/share/doc/libjpeg-devel-6b


class JpgInput : public ImageInput {
 public:
    JpgInput () { init(); }
    virtual ~JpgInput () { close(); }
    virtual const char * format_name (void) const { return "JPEG"; }
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

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress (&cinfo);            // initialize decompressor
    jpeg_stdio_src (&cinfo, fd);                // specify the data source
    jpeg_read_header (&cinfo, FALSE);           // read the file parameters
    jpeg_start_decompress (&cinfo);             // start working
    first_scanline = true;                      // start decompressor

    spec = ImageIOFormatSpec();
    spec.x = 0;
    spec.y = 0;
    spec.z = 0;
    spec.width = cinfo.output_width;
    spec.height = cinfo.output_height;
    spec.nchannels = cinfo.output_components;
    spec.depth = 1;
    spec.full_width = spec.width;
    spec.full_height = spec.height;
    spec.full_depth = spec.depth;
    spec.set_format (PT_UINT8);
    spec.tile_width = 0;
    spec.tile_height = 0;
    spec.tile_depth = 0;

    spec.channelnames.clear();
    switch (spec.nchannels) {
    case 1:
        spec.channelnames.push_back("a");
        break;
    case 3:
        spec.channelnames.push_back("r");
        spec.channelnames.push_back("g");
        spec.channelnames.push_back("b");
        break;
    case 4:
        spec.channelnames.push_back("r");
        spec.channelnames.push_back("g");
        spec.channelnames.push_back("b");
        spec.channelnames.push_back("a");
        break;
    default:
        fclose (fd);
        return false;
    }
    newspec = spec;
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

