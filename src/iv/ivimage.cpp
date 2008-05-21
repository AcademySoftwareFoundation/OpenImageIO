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

#include <boost/foreach.hpp>
#include <boost/scoped_ptr.hpp>

#include "imageviewer.h"
#include "strutil.h"



IvImage::IvImage (const std::string &filename)
    : m_name(filename), m_pixels(NULL), m_thumbnail(NULL),
      m_spec_valid(false), m_pixels_valid(false), m_thumbnail_valid(false),
      m_badfile(false), m_gamma(1), m_exposure(0)
{
}



IvImage::~IvImage ()
{
    delete [] m_pixels;
    delete [] m_thumbnail;
}



bool
IvImage::init_spec (const std::string &filename)
{
    m_name = filename;
    ImageInput *in = ImageInput::create (filename.c_str(), "" /* searchpath */);
    if (in->open (filename.c_str(), m_spec)) {
        in->close ();
        m_badfile = false;
        m_spec_valid = true;
        std::cerr << "init_spec succeeded " << filename << "\n";
    } else {
        m_badfile = true;
        m_spec_valid = false;
        delete in;
    }
    return !m_badfile;
}



bool
IvImage::read (bool force, OpenImageIO::ProgressCallback progress_callback,
               void *progress_callback_data)
{
    // Don't read if we already have it in memory, unless force is true.
    // FIXME: should we also check the time on the file to see if it's
    // been updated since we last loaded?
    if (m_pixels && m_pixels_valid && !force)
        return true;

    // Find an ImageIO plugin that can open the input file, and open it.
    boost::scoped_ptr<ImageInput> in (ImageInput::create (m_name.c_str(), "" /* searchpath */));
    if (! in) {
        m_err = OpenImageIO::error_message().c_str();
        return false;
    }
    if (! in->open (m_name.c_str(), m_spec)) {
        m_err = OpenImageIO::error_message().c_str();
        return false;
    }
    delete [] m_pixels;
    m_pixels = new char [m_spec.image_bytes()];
    const int as = OpenImageIO::AutoStride;
    bool ok = in->read_image (m_spec.format, m_pixels, as, as, as,
                              progress_callback, progress_callback_data);
    if (ok)
        m_pixels_valid = true;
    in->close ();
    return ok;
}
