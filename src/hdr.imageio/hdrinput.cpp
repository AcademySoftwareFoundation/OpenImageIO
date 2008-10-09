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
using namespace OpenImageIO;
#include "fmath.h"
#include "rgbe.h"



/////////////////////////////////////////////////////////////////////////////
// .hdr / .rgbe files - HDR files from Radiance
//
// General info on the hdr/rgbe format can be found at:
//     http://local.wasp.uwa.edu.au/~pbourke/dataformats/pic/
// The source code in rgbe.{h,cpp} originally came from:
//     http://www.graphics.cornell.edu/~bjw/rgbe.html
// Also see Greg Ward's "Real Pixels" chapter in Graphics Gems II for an
// explanation of the encoding that's used in Radiance rgba files.
/////////////////////////////////////////////////////////////////////////////



class HdrInput : public ImageInput {
public:
    HdrInput () { init(); }
    virtual ~HdrInput () { close(); }
    virtual const char * format_name (void) const { return "hdr"; }
    virtual bool open (const char *name, ImageSpec &spec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int index, ImageSpec &newspec);

private:
    std::string m_filename;       ///< File name
    FILE *m_fd;                   ///< The open file handle
    int m_subimage;               ///< What subimage are we looking at?
    int m_nextscanline;           ///< Next scanline to read

    void init () {
        m_fd = NULL;
        m_subimage = -1;
        m_nextscanline = 0;
    }

};



// Export version number and create function symbols
extern "C" {
    DLLEXPORT int imageio_version = IMAGEIO_VERSION;
    DLLEXPORT HdrInput *hdr_input_imageio_create () {
        return new HdrInput;
    }
    DLLEXPORT const char *hdr_input_extensions[] = {
        "hdr", "rgbe", NULL
    };
};



bool
HdrInput::open (const char *name, ImageSpec &newspec)
{
    m_filename = name;
    return seek_subimage (0, newspec);
}



bool
HdrInput::seek_subimage (int index, ImageSpec &newspec)
{
    if (index != 0)
        return false;

    close();

    // Check that file exists and can be opened
    m_fd = fopen (m_filename.c_str(), "rb");
    if (m_fd == NULL)
        return false;

    rgbe_header_info h;
    int width, height;
    int r = RGBE_ReadHeader (m_fd, &width, &height, &h);
    if (r != RGBE_RETURN_SUCCESS) {
        close ();
        return false;
    }

    m_spec = ImageSpec (width, height, 3, TypeDesc::FLOAT);

    if (h.valid & RGBE_VALID_GAMMA)
        m_spec.gamma = h.gamma;
    if (h.valid & RGBE_VALID_ORIENTATION)
        m_spec.attribute ("Orientation", h.orientation);

    // FIXME -- should we do anything about exposure, software,
    // pixaspect, primaries?  (N.B. rgbe.c doesn't even handle most of them)

    m_subimage = index;
    m_nextscanline = 0;
    newspec = m_spec;
    return true;
}



bool
HdrInput::read_native_scanline (int y, int z, void *data)
{
    if (y != m_nextscanline)
        return false;
    ++m_nextscanline;
    int r = RGBE_ReadPixels_RLE (m_fd, (float *)data, m_spec.width, 1);
    return (r == RGBE_RETURN_SUCCESS);
}



bool
HdrInput::close ()
{
    if (m_fd)
        fclose (m_fd);
    init ();   // Reset to initial state
    return true;
}

