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

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include "rgbe.h"


OIIO_PLUGIN_NAMESPACE_BEGIN

/////////////////////////////////////////////////////////////////////////////
// .hdr / .rgbe files - HDR files from Radiance
//
// General info on the hdr/rgbe format can be found at:
//     http://local.wasp.uwa.edu.au/~pbourke/dataformats/pic/
// The source code in rgbe.{h,cpp} originally came from:
//     http://www.graphics.cornell.edu/~bjw/rgbe.html
// But it's been modified in several minor ways by LG.
// Also see Greg Ward's "Real Pixels" chapter in Graphics Gems II for an
// explanation of the encoding that's used in Radiance rgba files.
/////////////////////////////////////////////////////////////////////////////



class HdrInput final : public ImageInput {
public:
    HdrInput () { init(); }
    virtual ~HdrInput () { close(); }
    virtual const char * format_name (void) const { return "hdr"; }
    virtual bool open (const std::string &name, ImageSpec &spec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int subimage, int miplevel, ImageSpec &newspec);

private:
    std::string m_filename;       ///< File name
    FILE *m_fd;                   ///< The open file handle
    int m_subimage;               ///< What subimage are we looking at?
    int m_next_scanline;          ///< Next scanline to read
    char rgbe_error[1024];        ///< Buffer for RGBE library error msgs

    void init () {
        m_fd = NULL;
        m_subimage = -1;
        m_next_scanline = 0;
    }

};



// Export version number and create function symbols
OIIO_PLUGIN_EXPORTS_BEGIN

    OIIO_EXPORT int hdr_imageio_version = OIIO_PLUGIN_VERSION;
    OIIO_EXPORT const char* hdr_imageio_library_version () { return NULL; }
    OIIO_EXPORT ImageInput *hdr_input_imageio_create () {
        return new HdrInput;
    }
    OIIO_EXPORT const char *hdr_input_extensions[] = {
        "hdr", "rgbe", NULL
    };

OIIO_PLUGIN_EXPORTS_END



bool
HdrInput::open (const std::string &name, ImageSpec &newspec)
{
    m_filename = name;
    return seek_subimage (0, 0, newspec);
}



bool
HdrInput::seek_subimage (int subimage, int miplevel, ImageSpec &newspec)
{
    // HDR doesn't support multiple subimages or mipmaps
    if (subimage != 0 || miplevel != 0)
        return false;

    // Skip the hard work if we're already on the requested subimage
    if (subimage == current_subimage()) {
        newspec = spec();
        return true;
    }

    close();

    // Check that file exists and can be opened
    m_fd = Filesystem::fopen (m_filename, "rb");
    if (m_fd == NULL) {
        error ("Could not open file \"%s\"", m_filename.c_str());
        return false;
    }

    rgbe_header_info h;
    int width, height;
    int r = RGBE_ReadHeader (m_fd, &width, &height, &h, rgbe_error);
    if (r != RGBE_RETURN_SUCCESS) {
        error ("%s", rgbe_error);
        close ();
        return false;
    }

    m_spec = ImageSpec (width, height, 3, TypeDesc::FLOAT);

    if (h.valid & RGBE_VALID_GAMMA) {
        // Round gamma to the nearest hundredth to prevent stupid
        // precision choices and make it easier for apps to make
        // decisions based on known gamma values. For example, you want
        // 2.2, not 2.19998.
        float g = float (1.0 / h.gamma);
        g = roundf (100.0 * g) / 100.0f;
        m_spec.attribute ("oiio:Gamma", g);
        if (g == 1.0f)
            m_spec.attribute ("oiio:ColorSpace", "linear");
        else
            m_spec.attribute ("oiio:ColorSpace",
                              Strutil::format("GammaCorrected%.2g", g));
    } else {
        // Presume linear color space
        m_spec.attribute ("oiio:ColorSpace", "linear");
    }
    if (h.valid & RGBE_VALID_ORIENTATION)
        m_spec.attribute ("Orientation", h.orientation);

    // FIXME -- should we do anything about exposure, software,
    // pixaspect, primaries?  (N.B. rgbe.c doesn't even handle most of them)

    m_subimage = subimage;
    m_next_scanline = 0;
    newspec = m_spec;
    return true;
}



bool
HdrInput::read_native_scanline (int y, int z, void *data)
{
    if (m_next_scanline > y) {
        // User is trying to read an earlier scanline than the one we're
        // up to.  Easy fix: close the file and re-open.
        ImageSpec dummyspec;
        int subimage = current_subimage();
        int miplevel = current_miplevel();
        if (! close ()  ||
            ! open (m_filename, dummyspec)  ||
            ! seek_subimage (subimage, miplevel, dummyspec))
            return false;    // Somehow, the re-open failed
        assert (m_next_scanline == 0 && current_subimage() == subimage &&
                current_miplevel() == miplevel);
    }
    while (m_next_scanline <= y) {
        // Keep reading until we're read the scanline we really need
        int r = RGBE_ReadPixels_RLE (m_fd, (float *)data, m_spec.width, 1, rgbe_error);
        ++m_next_scanline;
        if (r != RGBE_RETURN_SUCCESS) {
            error ("%s", rgbe_error);
            return false;
        }
    }
    return true;
}



bool
HdrInput::close ()
{
    if (m_fd)
        fclose (m_fd);
    init ();   // Reset to initial state
    return true;
}

OIIO_PLUGIN_NAMESPACE_END

