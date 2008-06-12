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
    if (! in) {
        std::cerr << OpenImageIO::error_message() << "\n";
    }
    if (in && in->open (filename.c_str(), m_spec)) {
        in->close ();
        m_badfile = false;
        m_spec_valid = true;
    } else {
        m_badfile = true;
        m_spec_valid = false;
        delete in;
    }
    m_shortinfo.clear();  // invalidate info strings
    m_longinfo.clear();
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

    // invalidate info strings
    m_shortinfo.clear();
    m_longinfo.clear();

    // Find an ImageIO plugin that can open the input file, and open it.
    boost::scoped_ptr<ImageInput> in (ImageInput::create (m_name.c_str(), "" /* searchpath */));
    if (! in) {
        m_err = OpenImageIO::error_message().c_str();
        return false;
    }
    if (! in->open (m_name.c_str(), m_spec)) {
        m_err = in->error_message();
        return false;
    }
    delete [] m_pixels;
    m_pixels = new char [m_spec.image_bytes()];
    const OpenImageIO::stride_t as = OpenImageIO::AutoStride;
    bool ok = in->read_image (m_spec.format, m_pixels, as, as, as,
                              progress_callback, progress_callback_data);
    if (ok)
        m_pixels_valid = true;
    else {
        m_err = in->error_message();
    }
    in->close ();
    if (progress_callback)
        progress_callback (progress_callback_data, 0);
    return ok;
}



std::string
IvImage::shortinfo () const
{
    if (m_shortinfo.empty()) {
        m_shortinfo = Strutil::format ("%d x %d", m_spec.width, m_spec.height);
        if (m_spec.depth > 1)
            m_shortinfo += Strutil::format (" x %d", m_spec.depth);
        m_shortinfo += Strutil::format (" x %d channel %s (%.2f MB)",
                                        m_spec.nchannels,
                                        ParamBaseTypeNameString(m_spec.format),
                                        (float)m_spec.image_bytes() / (1024.0*1024.0));
    }
    return m_shortinfo;
}



// Format name/value pairs as HTML table entries.
static std::string
infoline (const char *name, const std::string &value)
{
    std::string line = Strutil::format ("<tr><td><i>%s</i> : &nbsp;&nbsp;</td>",
                                        name);
    line += Strutil::format ("<td>%s</td></tr>\n", value.c_str());
    return line;
}

static std::string
infoline (const char *name, int value)
{
    return infoline (name, Strutil::format ("%d", value));
}



std::string
IvImage::longinfo () const
{
    using Strutil::format;  // shorthand
    if (m_longinfo.empty()) {
        m_longinfo += "<table>";
//        m_longinfo += infoline (format("<b>%s</b>", m_name.c_str()).c_str(),
//                                std::string());
        if (m_spec.depth <= 1)
            m_longinfo += infoline ("Dimensions", 
                        format ("%d x %d pixels", m_spec.width, m_spec.height));
        else
            m_longinfo += infoline ("Dimensions", 
                        format ("%d x %d x %d pixels",
                                m_spec.width, m_spec.height, m_spec.depth));
        m_longinfo += infoline ("Channels", m_spec.nchannels);
        // FIXME: put all channel names in the table
        m_longinfo += infoline ("Data format", ParamBaseTypeNameString(m_spec.format));
        m_longinfo += infoline ("Data size",
             format("%.2f MB", (float)m_spec.image_bytes() / (1024.0*1024.0)));
        m_longinfo += infoline ("Image origin", 
                                format ("%d, %d, %d", m_spec.x, m_spec.y, m_spec.z));
        m_longinfo += infoline ("Full uncropped size", 
                                format ("%d x %d x %d", m_spec.full_width, m_spec.full_height, m_spec.full_depth));
        if (m_spec.tile_width)
            m_longinfo += infoline ("Scanline/tile",
                            format ("tiled %d x %d x %d", m_spec.tile_width,
                                    m_spec.tile_height, m_spec.tile_depth));
        else
            m_longinfo += infoline ("Scanline/tile", "scanline");
        if (m_spec.alpha_channel >= 0)
            m_longinfo += infoline ("Alpha channel", m_spec.alpha_channel);
        if (m_spec.z_channel >= 0)
            m_longinfo += infoline ("Depth (z) channel", m_spec.z_channel);
        // gamma
        // image format

        BOOST_FOREACH (const ImageIOParameter &p, m_spec.extra_params) {
            if (p.type == PT_STRING)
                m_longinfo += infoline (p.name.c_str(), *(const char **)p.data());
            else if (p.type == PT_FLOAT)
                m_longinfo += infoline (p.name.c_str(), format("%g",*(const float *)p.data()));
            else if (p.type == PT_INT)
                m_longinfo += infoline (p.name.c_str(), *(const int *)p.data());
            else if (p.type == PT_UINT)
                m_longinfo += infoline (p.name.c_str(), format("%u",*(const unsigned int *)p.data()));
            else
                m_longinfo += infoline (p.name.c_str(), "<unknown data type>");
        }

        m_longinfo += "</table>";
    }
    return m_longinfo;
}
