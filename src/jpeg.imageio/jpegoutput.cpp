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
#include <cstdlib>
#include <cstring>
#include <ctype.h>
#include <cstdio>
#include <csetjmp>

extern "C" {
#include "jpeglib.h"
#include "tiff.h"
}

#include "imageio.h"
#include "fmath.h"

using namespace OpenImageIO;


// See JPEG library documentation in /usr/share/doc/libjpeg-devel-6b


class JpgOutput : public ImageOutput {
 public:
    JpgOutput () { init(); }
    virtual ~JpgOutput () { close(); }
    virtual const char * format_name (void) const { return "jpeg"; }
    virtual bool supports (const char *property) const { return false; }
    virtual bool open (const char *name, const ImageIOFormatSpec &spec,
                       bool append=false);
    virtual bool write_scanline (int y, int z, ParamBaseType format,
                                 const void *data, stride_t xstride);
    bool close ();
 private:
    FILE *fd;
    std::vector<unsigned char> scratch;
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    void init (void) { fd = NULL; }
};



extern "C" {
    DLLEXPORT JpgOutput *jpeg_output_imageio_create () {
        return new JpgOutput;
    }
    DLLEXPORT const char *jpeg_output_extensions[] = {
        "jpg", "jpe", "jpeg", NULL
    };
};




bool
JpgOutput::open (const char *name, const ImageIOFormatSpec &newspec,
                 bool append)
{
    if (append) {
        error ("JPG doesn't support multiple images per file");
        return false;
    }

    fd = fopen (name, "wb");
    if (fd == NULL) {
        error ("Unable to open file");
        return false;
    }

    // Save spec for later use
    m_spec = newspec;

    int quality = 98;

    cinfo.err = jpeg_std_error (&jerr);                 // set error handler
    jpeg_create_compress (&cinfo);                      // create compressor
    jpeg_stdio_dest (&cinfo, fd);                       // set output stream

    // set compression parameters
    cinfo.image_width = m_spec.width;
    cinfo.image_height = m_spec.height;

    if (m_spec.nchannels == 3 || m_spec.nchannels == 4) {
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;
        m_spec.nchannels = 3;  // Force RGBA -> RGB
        m_spec.alpha_channel = -1;  // No alpha channel
    } else if (m_spec.nchannels == 1) {
        cinfo.input_components = 1;
        cinfo.in_color_space = JCS_GRAYSCALE;
    }

    jpeg_set_defaults (&cinfo);                         // default compression
    jpeg_set_quality (&cinfo, quality, TRUE);           // baseline values
    jpeg_start_compress (&cinfo, TRUE);                 // start working

    std::vector<char> exif;
    exif.reserve (0xffff);   // So we can point to the middle without realloc
    exif.push_back ('E');
    exif.push_back ('x');
    exif.push_back ('i');
    exif.push_back ('f');
    exif.push_back (0);
    exif.push_back (0);
    int tiffstart = exif.size();
    TIFFHeader *head = &exif[exif.size()];
    exif.resize (exif.size() + sizeof(TIFFHeader));
    bool host_little = littleendian();
    head->tiff_magic = host_little ? 0x4949 : 0x4d4d;
    head->tiff_version = 42;
    head->tiff_diroff = exif.size() - tiffstart;
    unsigned short *ndirs = (unsigned short) &exif[exif.size()];
    exif.resize (exif.size() + sizeof(*ndirs));
    *ndirs = 1;
    TIFFDirEntry *dir = (TIFFDirEntry *) &exif[exif.size()];
    exif.resize (exif.size() + sizeof(*dir));
    dir->tdir_tag = TIFFTAG_EXIFID;
    dir->tdir_type = TIFFTAG_IFD;  // ?? right?
    dir->tdir_count = 1;
    dir->tdir_offset = exif.size();
#if 0
    XXX I am here
    unsigned short *ndirs = (unsigned short) &exif[exif.size()];
    exif.resize (exif.size() + sizeof(*ndirs));
    *ndirs = 1;
    TIFFDirEntry *dir = (TIFFDirEntry *) &exif[exif.size()];
    exif.resize (exif.size() + sizeof(*dir));
    dir->tdir_tag = TIFFTAG_EXIFID;
    dir->tdir_type = TIFFTAG_IFD;  // ?? right?
    dir->tdir_count = 1;
    dir->tdir_offset = exif.size();
#endif

//    jpeg_write_marker (&cinfo, JPEG_APP0+1, &exif[0], exif.size());
//    jpeg_write_marker (&cinfo, JPEG_COM, comment, strlen(comment) /* + 1 ? */ );

    m_spec.set_format (PT_UINT8);  // JPG is only 8 bit

    return true;
}



bool
JpgOutput::write_scanline (int y, int z, ParamBaseType format,
                           const void *data, stride_t xstride)
{
    y -= m_spec.y;
    assert (y == (int)cinfo.next_scanline);
    assert (y < (int)cinfo.image_height);

    data = to_native_scanline (format, data, xstride, scratch);

    jpeg_write_scanlines (&cinfo, (JSAMPLE**)&data, 1);

    return true;
}



bool
JpgOutput::close ()
{
    jpeg_finish_compress (&cinfo);
    jpeg_destroy_compress (&cinfo);
    fclose (fd);
    init();
    
    return true;
}

