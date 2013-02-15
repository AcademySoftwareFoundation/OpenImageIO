/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
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
#include <iostream>

#include "imageio.h"
#include "filesystem.h"
#include "fmath.h"
#include "rgbe.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

class HdrOutput : public ImageOutput {
 public:
    HdrOutput () { init(); }
    virtual ~HdrOutput () { close(); }
    virtual const char * format_name (void) const { return "hdr"; }
    virtual bool supports (const std::string &property) const { return false; }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode);
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    bool close ();
 private:
    FILE *m_fd;
    std::vector<unsigned char> scratch;
    char rgbe_error[1024];        ///< Buffer for RGBE library error msgs

    void init (void) { m_fd = NULL; }
};


OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT ImageOutput *hdr_output_imageio_create () {
        return new HdrOutput;
    }
    OIIO_EXPORT const char *hdr_output_extensions[] = {
        "hdr", "rgbe", NULL
    };

OIIO_PLUGIN_EXPORTS_END


bool
HdrOutput::open (const std::string &name, const ImageSpec &newspec,
                 OpenMode mode)
{
    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    // Save spec for later use
    m_spec = newspec;
    // HDR always behaves like floating point
    m_spec.set_format (TypeDesc::FLOAT);

    // Check for things HDR can't support
    if (m_spec.nchannels != 3) {
        error ("HDR can only support 3-channel images");
        return false;
    }
    if (m_spec.width < 1 || m_spec.height < 1) {
        error ("Image resolution must be at least 1x1, you asked for %d x %d",
               m_spec.width, m_spec.height);
        return false;
    }
    if (m_spec.depth < 1)
        m_spec.depth = 1;
    if (m_spec.depth > 1) {
        error ("%s does not support volume images (depth > 1)", format_name());
        return false;
    }

    m_spec.set_format (TypeDesc::FLOAT);   // Native rgbe is float32 only

    m_fd = Filesystem::fopen (name, "wb");
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
    p = m_spec.find_attribute ("Orientation", TypeDesc::INT);
    if (p) {
        h.valid |= RGBE_VALID_ORIENTATION;
        h.orientation = * (int *)p->data();
    }

    // FIXME -- should we do anything about gamma, exposure, software,
    // pixaspect, primaries?  (N.B. rgbe.c doesn't even handle most of them)

    int r = RGBE_WriteHeader (m_fd, m_spec.width, m_spec.height, &h, rgbe_error);
    if (r != RGBE_RETURN_SUCCESS)
        error ("%s", rgbe_error);

    return true;
}



bool
HdrOutput::write_scanline (int y, int z, TypeDesc format,
                           const void *data, stride_t xstride)
{
    data = to_native_scanline (format, data, xstride, scratch);
    int r = RGBE_WritePixels_RLE (m_fd, (float *)data, m_spec.width, 1, rgbe_error);
    if (r != RGBE_RETURN_SUCCESS)
        error ("%s", rgbe_error);
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

OIIO_PLUGIN_NAMESPACE_END

