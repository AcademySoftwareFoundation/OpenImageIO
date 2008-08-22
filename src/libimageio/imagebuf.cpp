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

#include <iostream>

#include <ImathFun.h>
#include <half.h>
#include <boost/scoped_ptr.hpp>

#include "imageio.h"
#define DLL_EXPORT_PUBLIC /* Because we are implementing ImageBuf */
#include "imagebuf.h"
#undef DLL_EXPORT_PUBLIC
#include "dassert.h"
#include "fmath.h"


namespace OpenImageIO {



ImageBuf::ImageBuf (const std::string &filename)
    : m_name(filename), m_nsubimages(0), m_current_subimage(0),
      m_spec_valid(false), m_badfile(false), m_orientation(1)
{
}



ImageBuf::ImageBuf (const std::string &filename, const ImageSpec &spec)
    : m_name(filename), m_nsubimages(0), m_current_subimage(0),
      m_spec_valid(false), m_badfile(false), m_orientation(1)
{
    alloc (spec);
}



ImageBuf::~ImageBuf ()
{
}



void
ImageBuf::realloc ()
{
    size_t newsize = spec().image_bytes ();
    if (((int)newsize - (int)m_pixels.size()) > 1024*1024) {
        // If we are substantially shrinking, try to actually free
        // memory, which std::vector::resize does not do!
        std::vector<char> tmp;      // vector with 0 memory
        std::swap (tmp, m_pixels);  // Now tmp holds the mem, not m_pixels
        // As tmp leaves scope, it frees m_pixels's old memory
    }
    m_pixels.resize (newsize);
}



void
ImageBuf::alloc (const ImageSpec &spec)
{
    m_spec = spec;
    m_spec_valid = true;
    realloc ();
}



bool
ImageBuf::init_spec (const std::string &filename)
{
    m_name = filename;
    boost::scoped_ptr<ImageInput> in (ImageInput::create (filename.c_str(), "" /* searchpath */));
    if (! in) {
        std::cerr << OpenImageIO::error_message() << "\n";
    }
    if (in && in->open (filename.c_str(), m_spec)) {
        ImageSpec tempspec;
        m_nsubimages = 1;
        while (in->seek_subimage (m_nsubimages, tempspec))
            ++m_nsubimages;
        std::cerr << filename << " has " << m_nsubimages << " subimages\n";
        m_current_subimage = 0;
        in->close ();
        m_badfile = false;
        m_spec_valid = true;
    } else {
        m_badfile = true;
        m_spec_valid = false;
    }
    return !m_badfile;
}



bool
ImageBuf::read (int subimage, bool force,
               OpenImageIO::ProgressCallback progress_callback,
               void *progress_callback_data)
{
    // Find an ImageIO plugin that can open the input file, and open it.
    boost::scoped_ptr<ImageInput> in (ImageInput::create (m_name.c_str(), "" /* searchpath */));
    if (! in) {
        m_err = OpenImageIO::error_message();
        return false;
    }
    if (in->open (m_name.c_str(), m_spec)) {
        ImageSpec tempspec;
        m_nsubimages = 1;
        while (in->seek_subimage (m_nsubimages, tempspec))
            ++m_nsubimages;
        m_current_subimage = 0;
        in->seek_subimage (0, m_spec);
        m_badfile = false;
        m_spec_valid = true;
    } else {
        m_badfile = true;
        m_spec_valid = false;
        m_err = in->error_message();
        return false;
    }

    if (subimage > 0 &&  in->seek_subimage (subimage, m_spec))
        m_current_subimage = subimage;
    else
        m_current_subimage = 0;

    ImageIOParameter *orient = m_spec.find_attribute ("orientation", PT_UINT);
    m_orientation = orient ? *(unsigned int *)orient->data() : 1;

    realloc ();
    const OpenImageIO::stride_t as = OpenImageIO::AutoStride;
    bool ok = in->read_image (m_spec.format, &m_pixels[0], as, as, as,
                              progress_callback, progress_callback_data);
    if (! ok)
        m_err = in->error_message();
    in->close ();
    if (progress_callback)
        progress_callback (progress_callback_data, 0);
    return ok;
}



bool
ImageBuf::save (const std::string &_filename, const std::string &_fileformat,
                OpenImageIO::ProgressCallback progress_callback,
                void *progress_callback_data)
{
    std::string filename = _filename.size() ? _filename : name();
    std::string fileformat = _fileformat.size() ? _fileformat : filename;
    boost::scoped_ptr<ImageOutput> out (ImageOutput::create (fileformat.c_str(), "" /* searchpath */));
    if (! out) {
        m_err = OpenImageIO::error_message();
        return false;
    }
    if (! out->open (filename.c_str(), m_spec)) {
        m_err = out->error_message();
        return false;
    }
    OpenImageIO::stride_t as = OpenImageIO::AutoStride;
    if (! out->write_image (m_spec.format, &m_pixels[0], as, as, as,
                            progress_callback, progress_callback_data)) {
        m_err = out->error_message();
        return false;
    }
    out->close ();
    if (progress_callback)
        progress_callback (progress_callback_data, 0);
    return true;
}



float
ImageBuf::getchannel (int x, int y, int c) const
{
    if (c < 0 || c >= spec().nchannels)
        return 0.0f;
    const void *pixel = pixeladdr(x,y);
    switch (spec().format) {
    case PT_FLOAT:
        return ((float *)pixel)[c];
    case PT_HALF:
        return ((half *)pixel)[c];
    case PT_DOUBLE:
        return ((double *)pixel)[c];
    case PT_INT8:
        return ((char *)pixel)[c] / (float)std::numeric_limits<char>::max();
    case PT_UINT8:
        return ((unsigned char *)pixel)[c] / (float)std::numeric_limits<unsigned char>::max();
    case PT_INT16:
        return ((short *)pixel)[c] / (float)std::numeric_limits<short>::max();
    case PT_UINT16:
        return ((unsigned short *)pixel)[c] / 
                (float)std::numeric_limits<unsigned short>::max();
    default:
        ASSERT (0);
        return 0.0f;
    }
}



void
ImageBuf::getpixel (int x, int y, float *pixel, int maxchannels) const
{
    int n = std::min (spec().nchannels, maxchannels);
    OpenImageIO::convert_types (spec().format, pixeladdr(x,y),
                                PT_FLOAT, pixel, n);
}



void
ImageBuf::interppixel (float x, float y, float *pixel) const
{
    const int maxchannels = 64;  // Reasonable guess
    float p[4][maxchannels];
    int n = std::min (spec().nchannels, maxchannels);
    x -= 0.5f;
    y -= 0.5f;
    int xtexel, ytexel;
    float xfrac, yfrac;
    xfrac = floorfrac (x, &xtexel);
    yfrac = floorfrac (y, &ytexel);
    int xtexel0 = Imath::clamp (xtexel, xmin(), xmax());
    int ytexel0 = Imath::clamp (ytexel, ymin(), ymax());
    int xtexel1 = Imath::clamp (xtexel+1, xmin(), xmax());
    int ytexel1 = Imath::clamp (ytexel+1, ymin(), ymax());
    getpixel (xtexel0, ytexel0, p[0], n);
    getpixel (xtexel1, ytexel0, p[1], n);
    getpixel (xtexel0, ytexel1, p[2], n);
    getpixel (xtexel1, ytexel1, p[3], n);
    bilerp (p[0], p[1], p[2], p[3], xfrac, yfrac, n, pixel);
}



void
ImageBuf::setpixel (int x, int y, const float *pixel, int maxchannels)
{
    int n = std::min (spec().nchannels, maxchannels);
    OpenImageIO::convert_types (PT_FLOAT, pixel, 
                                spec().format, pixeladdr(x,y), n);
}



int
ImageBuf::oriented_width () const
{
    return m_orientation <= 4 ? m_spec.width : m_spec.height;
}



int
ImageBuf::oriented_height () const
{
    return m_orientation <= 4 ? m_spec.height : m_spec.width;
}



void
ImageBuf::zero ()
{
    memset (&m_pixels[0], 0, m_pixels.size());
}


};  // end namespace OpenImageIO
