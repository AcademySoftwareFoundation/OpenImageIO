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
    std::vector<float> m_pixels;  ///< Data buffer

    void init () {
        m_fd = NULL;
        m_subimage = -1;
        m_pixels.clear ();
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
    // Not sure what to do with the header's "exposure" field

    m_subimage = index;
    newspec = m_spec;
    return true;
}



bool
HdrInput::read_native_scanline (int y, int z, void *data)
{
    if (! m_pixels.size()) {    // We haven't read the pixels yet
        m_pixels.resize (m_spec.width * m_spec.height * m_spec.nchannels);
        RGBE_ReadPixels_RLE (m_fd, &m_pixels[0], m_spec.width, m_spec.height);
    }
    memcpy (data, &m_pixels[y * m_spec.width * m_spec.nchannels],
            m_spec.scanline_bytes());
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

