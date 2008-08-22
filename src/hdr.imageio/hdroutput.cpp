/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (this is the MIT license)
/////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <cstdio>
#include <iostream>

#include "imageio.h"
using namespace OpenImageIO;
#include "fmath.h"
#include "rgbe.h"



class HdrOutput : public ImageOutput {
 public:
    HdrOutput () { init(); }
    virtual ~HdrOutput () { close(); }
    virtual const char * format_name (void) const { return "hdr"; }
    virtual bool supports (const char *property) const { return false; }
    virtual bool open (const char *name, const ImageSpec &spec,
                       bool append=false);
    virtual bool write_scanline (int y, int z, ParamBaseType format,
                                 const void *data, stride_t xstride);
    bool close ();
 private:
    FILE *m_fd;
    std::vector<unsigned char> scratch;

    void init (void) { m_fd = NULL; }
};



extern "C" {
    DLLEXPORT HdrOutput *hdr_output_imageio_create () {
        return new HdrOutput;
    }
    DLLEXPORT const char *hdr_output_extensions[] = {
        "hdr", "rgbe", NULL
    };
};




bool
HdrOutput::open (const char *name, const ImageSpec &newspec, bool append)
{
    if (append) {
        error ("HDR doesn't support multiple images per file");
        return false;
    }

    // Save spec for later use
    m_spec = newspec;

    // Check for things HDR can't support
    if (m_spec.nchannels != 3) {
        error ("HDR can only support 3-channel images");
        return false;
    }

    m_spec.set_format (PT_FLOAT);   // Native rgbe is float32 only

    m_fd = fopen (name, "wb");
    if (m_fd == NULL) {
        error ("Unable to open file");
        return false;
    }

    rgbe_header_info h;
    h.valid = 0;

    // Most readers seem to think that rgbe files are valid only if they
    // identify themselves as from "RADIANCE".
    h.valid |= RGBE_VALID_PROGRAMTYPE;
    strcpy (h.programtype, "RADIANCE");

    ImageIOParameter *p;
    p = m_spec.find_attribute ("Orientation", PT_INT);
    if (p) {
        h.valid |= RGBE_VALID_ORIENTATION;
        h.orientation = * (int *)p->data();
    }

    // FIXME -- should we do anything about gamma, exposure, software,
    // pixaspect, primaries?  (N.B. rgbe.c doesn't even handle most of them)

    RGBE_WriteHeader (m_fd, m_spec.width, m_spec.height, &h);

    return true;
}



bool
HdrOutput::write_scanline (int y, int z, ParamBaseType format,
                           const void *data, stride_t xstride)
{
    data = to_native_scanline (format, data, xstride, scratch);
    int r = RGBE_WritePixels_RLE (m_fd, (float *)data, m_spec.width, 1);
    return (r == RGBE_RETURN_SUCCESS);
}



bool
HdrOutput::close ()
{
    if (m_fd != NULL) {
        fclose (m_fd);
        m_fd = NULL;
    }
    init();

    return true;
}

