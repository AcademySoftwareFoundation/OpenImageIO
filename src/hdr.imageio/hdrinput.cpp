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
    virtual bool open (const char *name, ImageIOFormatSpec &spec);
    virtual bool read_native_scanline (int y, int z, void *data);
    virtual bool close ();
    virtual int current_subimage (void) const { return m_subimage; }
    virtual bool seek_subimage (int index, ImageIOFormatSpec &newspec);

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
HdrInput::open (const char *name, ImageIOFormatSpec &newspec)
{
    m_filename = name;
    return seek_subimage (0, newspec);
}



bool
HdrInput::seek_subimage (int index, ImageIOFormatSpec &newspec)
{
    if (index != 0)
        return false;

    close();

    // Check that file exists and can be opened
    m_fd = fopen (m_filename.c_str(), "rb");
    if (m_fd == NULL)
        return false;

    m_spec = ImageIOFormatSpec();

    rgbe_header_info h;
    int r = RGBE_ReadHeader (m_fd, &m_spec.width, &m_spec.height, &h);
    if (r != RGBE_RETURN_SUCCESS) {
        close ();
        return false;
    }

    m_spec.nchannels = 3;           // HDR files are always 3 channel
    m_spec.full_width = m_spec.width;
    m_spec.full_height = m_spec.height;
    m_spec.full_depth = m_spec.depth;
    m_spec.set_format (PT_FLOAT);   // HDR files are always float
    m_spec.channelnames.push_back ("R");
    m_spec.channelnames.push_back ("G");
    m_spec.channelnames.push_back ("B");

    if (h.valid & RGBE_VALID_GAMMA)
        m_spec.gamma = h.gamma;
    if (h.valid & RGBE_VALID_ORIENTATION)
        m_spec.attribute ("orientation", h.orientation);

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

