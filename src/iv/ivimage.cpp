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


#include <iostream>

#include <boost/foreach.hpp>
#include <boost/scoped_ptr.hpp>

#include "imageviewer.h"
#include "strutil.h"



IvImage::IvImage (const std::string &filename)
    : ImageBuf(filename), m_thumbnail(NULL),
      m_thumbnail_valid(false),
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

    bool ok = ImageBuf::read (subimage, force, TypeDesc::UNKNOWN,
                              progress_callback, progress_callback_data);
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
                                        m_spec.format.c_str(),
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
        const char *cspacename [] = { "unknown", "linear", "gamma %g", "sRGB" };
        m_longinfo += html_table_row ("Color space",
                  format (cspacename[(int)m_spec.linearity], m_spec.gamma));

        m_longinfo += html_table_row ("File format", file_format_name());
        m_longinfo += html_table_row ("Data format", m_spec.format.c_str());
        m_longinfo += html_table_row ("Data size",
             format("%.2f MB", (float)m_spec.image_bytes() / (1024.0*1024.0)));
        m_longinfo += html_table_row ("Image origin", 
                          format ("%d, %d, %d", m_spec.x, m_spec.y, m_spec.z));
        m_longinfo += html_table_row ("Full/display size", 
                          format ("%d x %d x %d", m_spec.full_width,
                                  m_spec.full_height, m_spec.full_depth));
        m_longinfo += html_table_row ("Full/display origin", 
                          format ("%d, %d, %d", m_spec.full_x,
                                  m_spec.full_y, m_spec.full_z));
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

        BOOST_FOREACH (const ImageIOParameter &p, m_spec.extra_attribs) {
            if (p.type() == TypeDesc::STRING)
                m_longinfo += html_table_row (p.name().c_str(), *(const char **)p.data());
            else if (p.type() == TypeDesc::FLOAT)
                m_longinfo += html_table_row (p.name().c_str(), format("%g",*(const float *)p.data()));
            else if (p.type() == TypeDesc::DOUBLE)
                m_longinfo += html_table_row (p.name().c_str(), format("%g",*(const double *)p.data()));
            else if (p.type() == TypeDesc::INT)
                m_longinfo += html_table_row (p.name().c_str(), *(const int *)p.data());
            else if (p.type() == TypeDesc::UINT)
                m_longinfo += html_table_row (p.name().c_str(), format("%u",*(const unsigned int *)p.data()));
            else if (p.type() == TypeDesc::INT16)
                m_longinfo += html_table_row (p.name().c_str(), *(const short *)p.data());
            else if (p.type() == TypeDesc::UINT16)
                m_longinfo += html_table_row (p.name().c_str(), format("%u",*(const unsigned short *)p.data()));
            else if (p.type() == TypeDesc::PT_MATRIX) {
                const float *m = (const float *)p.data();
                m_longinfo += html_table_row (p.name().c_str(),
                    format ("%g %g %g %g<br> %g %g %g %g<br> %g %g %g %g<br> %g %g %g %g",
                        m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], 
                        m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]));
            }
            else
                m_longinfo += html_table_row (p.name().c_str(),
                     format ("(unknown data type (base %d, agg %d vec %d)",
                             p.type().basetype, p.type().aggregate,
                             p.type().vecsemantics));
        }

        m_longinfo += "</table>";
    }
    return m_longinfo;
}
