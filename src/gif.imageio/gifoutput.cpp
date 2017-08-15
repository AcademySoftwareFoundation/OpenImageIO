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

#define USE_GIFH 1

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/platform.h>

namespace {
#define GIF_TEMP_MALLOC malloc
#define GIF_TEMP_FREE free
#define GIF_MALLOC malloc
#define GIF_FREE free
#include "gif.h"
}


OIIO_PLUGIN_NAMESPACE_BEGIN

class GIFOutput final : public ImageOutput {
 public:
    GIFOutput () { init(); }
    virtual ~GIFOutput () { close(); }
    virtual const char * format_name (void) const { return "gif"; }
    virtual int supports (string_view feature) const {
        return (feature == "alpha" ||
                feature == "random_access" ||
                feature == "multiimage" ||
                feature == "appendsubimage");
    }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool open (const std::string &name, int subimages,
                       const ImageSpec *specs);
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool close ();

 private:
    std::string m_filename;
    int m_subimage;                  // Current subimage index
    int m_nsubimages;
    bool m_pending_write;            // Do we have an image buffered?
    std::vector<ImageSpec> m_subimagespecs; // Saved subimage specs
    GifWriter m_gifwriter;
    std::vector<uint8_t> m_canvas;   // Image canvas, accumulating output
    int m_delay;

    void init (void) {
        m_filename.clear ();
        m_subimage = 0;
        m_canvas.clear ();
        m_pending_write = false;
    }

    bool start_subimage ();
    bool finish_subimage ();
};





OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT ImageOutput *gif_output_imageio_create () {
        return new GIFOutput;
    }
    OIIO_EXPORT const char *gif_output_extensions[] = { "gif", NULL };

OIIO_PLUGIN_EXPORTS_END



bool
GIFOutput::open (const std::string &name, const ImageSpec &newspec,
                 OpenMode mode)
{
    if (mode == Create) {
        return open (name, 1, &newspec);
    }

    if (mode == AppendMIPLevel) {
        error ("%s does not support MIP levels", format_name());
        return false;
    }

    if (mode == AppendSubimage) {
        if (m_pending_write)
            finish_subimage();
        ++m_subimage;
        m_spec = newspec;
        return start_subimage ();
    }

    ASSERTMSG (0, "Unknown open mode %d", int(mode));
    return false;
}



bool
GIFOutput::open (const std::string &name, int subimages,
                 const ImageSpec *specs)
{
    if (subimages < 1) {
        error ("%s does not support %d subimages.", format_name(), subimages);
        return false;
    }

    m_filename = name;
    m_subimage = 0;
    m_nsubimages = subimages;
    m_subimagespecs.assign (specs, specs+subimages);
    m_spec = specs[0];
    float fps = m_spec.get_float_attribute ("FramesPerSecond", 1.0f);
    m_delay = (fps == 0.0f ? 0 : (int)(100.0f/fps));
    return start_subimage ();
}



bool
GIFOutput::close ()
{
    if (m_pending_write) {
        finish_subimage ();
        GifEnd (&m_gifwriter);
    }
    init ();
    return true;
}



bool
GIFOutput::start_subimage ()
{
    // Check for things this format doesn't support
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
    if (m_spec.nchannels != 3 && m_spec.nchannels != 4) {
        error ("%s does not support %d-channel images",
               format_name(), m_spec.nchannels);
        return false;
    }

    m_spec.set_format (TypeDesc::UINT8);  // GIF is only 8 bit

    if (m_subimage == 0) {
        bool ok = GifBegin (&m_gifwriter, m_filename.c_str(),
                            m_spec.width, m_spec.height,
                            m_delay, 8 /*bit depth*/, true /*dither*/);
        if (!ok) {
            error ("Could not open file %s", m_filename);
            return false;
        }
    }
    m_canvas.clear ();
    m_canvas.resize (size_t(m_spec.image_pixels()*4), 255);

    m_pending_write = true;
    return true;
}



bool
GIFOutput::finish_subimage ()
{
    if (! m_pending_write)
        return true;

    bool ok = GifWriteFrame (&m_gifwriter, &m_canvas[0],
                             spec().width, spec().height,
                             m_delay, 8 /*bitdepth*/, true /*dither*/);
    m_pending_write = false;
    return ok;
}



bool
GIFOutput::write_scanline (int y, int z, TypeDesc format,
                           const void *data, stride_t xstride)
{
    return convert_image (spec().nchannels, spec().width, 1 /*1 scanline*/, 1,
                          data, format, xstride, AutoStride, AutoStride,
                          &m_canvas[y*spec().width*4], TypeDesc::UINT8,
                          4, AutoStride, AutoStride,
                          spec().nchannels > 3 ? 3 : -1/*alpha channel*/);

}


OIIO_PLUGIN_NAMESPACE_END
