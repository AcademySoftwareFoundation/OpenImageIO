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



IvImage::IvImage ()
    : m_pixels(NULL), m_thumbnail(NULL),
      m_pixels_valid(false), m_thumbnail_valid(false), m_badfile(false)
{
}



IvImage::~IvImage ()
{
    delete [] m_pixels;
    delete [] m_thumbnail;
}



bool
IvImage::read (const std::string &filename)
{
    m_name = filename;
    // Find an ImageIO plugin that can open the input file, and open it.
    boost::scoped_ptr<ImageInput> in (ImageInput::create (filename.c_str(), "" /* searchpath */));
    if (! in) {
        m_err = OpenImageIO::error_message().c_str();
        return false;
    }
    if (! in->open (filename.c_str(), m_spec)) {
        m_err = OpenImageIO::error_message().c_str();
        return false;
    }
    delete [] m_pixels;
    m_pixels = new char [m_spec.image_bytes()];
    in->read_image (m_spec.format, m_pixels);
    in->close ();
    return true;
}
