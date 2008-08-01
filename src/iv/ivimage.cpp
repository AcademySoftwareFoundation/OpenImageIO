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
    : ImageBuf(filename), m_thumbnail(NULL),
      m_pixels_valid(false), m_thumbnail_valid(false),
      m_gamma(1), m_exposure(0)
{
}



IvImage::~IvImage ()
{
    delete [] m_thumbnail;
}



bool
IvImage::init_spec (const std::string &filename)
{
    // invalidate info strings
    m_shortinfo.clear ();
    m_longinfo.clear ();

    return ImageBuf::init_spec (filename);
}



bool
IvImage::read (int subimage, bool force,
               OpenImageIO::ProgressCallback progress_callback,
               void *progress_callback_data)
{
    // Don't read if we already have it in memory, unless force is true.
    // FIXME: should we also check the time on the file to see if it's
    // been updated since we last loaded?
    if (m_pixels.size() && m_pixels_valid && !force && subimage == m_current_subimage)
        return true;

    // invalidate info strings
    m_shortinfo.clear();
    m_longinfo.clear();

    bool ok = ImageBuf::read (subimage, force, progress_callback,
                              progress_callback_data);
    m_pixels_valid = ok;
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
std::string
html_table_row (const char *name, const std::string &value)
{
    std::string line = Strutil::format ("<tr><td><i>%s</i> : &nbsp;&nbsp;</td>",
                                        name);
    line += Strutil::format ("<td>%s</td></tr>\n", value.c_str());
    return line;
}


std::string
html_table_row (const char *name, int value)
{
    return html_table_row (name, Strutil::format ("%d", value));
}


std::string
html_table_row (const char *name, float value)
{
    return html_table_row (name, Strutil::format ("%g", value));
}



std::string
IvImage::longinfo () const
{
    using Strutil::format;  // shorthand
    if (m_longinfo.empty()) {
        m_longinfo += "<table>";
//        m_longinfo += html_table_row (format("<b>%s</b>", m_name.c_str()).c_str(),
//                                std::string());
        if (m_spec.depth <= 1)
            m_longinfo += html_table_row ("Dimensions", 
                        format ("%d x %d pixels", m_spec.width, m_spec.height));
        else
            m_longinfo += html_table_row ("Dimensions", 
                        format ("%d x %d x %d pixels",
                                m_spec.width, m_spec.height, m_spec.depth));
        m_longinfo += html_table_row ("Channels", m_spec.nchannels);
        std::string chanlist;
        for (int i = 0;  i < m_spec.nchannels;  ++i) {
            chanlist += m_spec.channelnames[i].c_str();
            if (i != m_spec.nchannels-1)
                chanlist += ", ";
        }
        m_longinfo += html_table_row ("Channel list", chanlist);

        m_longinfo += html_table_row ("Data format", ParamBaseTypeNameString(m_spec.format));
        m_longinfo += html_table_row ("Data size",
             format("%.2f MB", (float)m_spec.image_bytes() / (1024.0*1024.0)));
        m_longinfo += html_table_row ("Image origin", 
                          format ("%d, %d, %d", m_spec.x, m_spec.y, m_spec.z));
        m_longinfo += html_table_row ("Full uncropped size", 
                          format ("%d x %d x %d", m_spec.full_width,
                                  m_spec.full_height, m_spec.full_depth));
        if (m_spec.tile_width)
            m_longinfo += html_table_row ("Scanline/tile",
                            format ("tiled %d x %d x %d", m_spec.tile_width,
                                    m_spec.tile_height, m_spec.tile_depth));
        else
            m_longinfo += html_table_row ("Scanline/tile", "scanline");
        if (m_spec.alpha_channel >= 0)
            m_longinfo += html_table_row ("Alpha channel", m_spec.alpha_channel);
        if (m_spec.z_channel >= 0)
            m_longinfo += html_table_row ("Depth (z) channel", m_spec.z_channel);
        // gamma
        // image format

        BOOST_FOREACH (const ImageIOParameter &p, m_spec.extra_attribs) {
            if (p.type == PT_STRING)
                m_longinfo += html_table_row (p.name.c_str(), *(const char **)p.data());
            else if (p.type == PT_FLOAT)
                m_longinfo += html_table_row (p.name.c_str(), format("%g",*(const float *)p.data()));
            else if (p.type == PT_INT)
                m_longinfo += html_table_row (p.name.c_str(), *(const int *)p.data());
            else if (p.type == PT_UINT)
                m_longinfo += html_table_row (p.name.c_str(), format("%u",*(const unsigned int *)p.data()));
            else
                m_longinfo += html_table_row (p.name.c_str(), "<unknown data type>");
        }

        m_longinfo += "</table>";
    }
    return m_longinfo;
}
