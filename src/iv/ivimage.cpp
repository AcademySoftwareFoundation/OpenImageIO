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
      m_badfile(false), m_gamma(1), m_exposure(0), m_orientation(1)
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
    boost::scoped_ptr<ImageInput> in (ImageInput::create (filename.c_str(), "" /* searchpath */));
    if (! in) {
        std::cerr << OpenImageIO::error_message() << "\n";
    }
    if (in && in->open (filename.c_str(), m_spec)) {
        ImageIOFormatSpec tempspec;
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
    m_shortinfo.clear();  // invalidate info strings
    m_longinfo.clear();
    return !m_badfile;
}



bool
IvImage::read (int subimage, bool force,
               OpenImageIO::ProgressCallback progress_callback,
               void *progress_callback_data)
{
    // Don't read if we already have it in memory, unless force is true.
    // FIXME: should we also check the time on the file to see if it's
    // been updated since we last loaded?
    if (m_pixels && m_pixels_valid && !force && subimage == m_current_subimage)
        return true;

    // invalidate info strings
    m_shortinfo.clear();
    m_longinfo.clear();

    // Find an ImageIO plugin that can open the input file, and open it.
    boost::scoped_ptr<ImageInput> in (ImageInput::create (m_name.c_str(), "" /* searchpath */));
    if (! in) {
        m_err = OpenImageIO::error_message();
        return false;
    }
    if (in->open (m_name.c_str(), m_spec)) {
        ImageIOFormatSpec tempspec;
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

    ImageIOParameter *orient = m_spec.find_parameter ("orientation");
    if (orient && orient->type == PT_UINT && orient->nvalues == 1)
        m_orientation = *(unsigned int *)orient->data();
    else 
        m_orientation = 1;

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



bool
IvImage::save (const std::string &filename,
               OpenImageIO::ProgressCallback progress_callback,
               void *progress_callback_data)
{
    std::cerr << "Save " << filename << "\n";
    boost::scoped_ptr<ImageOutput> out (ImageOutput::create (filename.c_str(), "" /* searchpath */));
    if (! out) {
        m_err = OpenImageIO::error_message();
        return false;
    }
    if (! out->open (filename.c_str(), m_spec)) {
        m_err = out->error_message();
        return false;
    }
    OpenImageIO::stride_t as = OpenImageIO::AutoStride;
    if (! out->write_image (m_spec.format, m_pixels, as, as, as,
                            progress_callback, progress_callback_data)) {
        m_err = out->error_message();
        return false;
    }
    out->close ();
    if (progress_callback)
        progress_callback (progress_callback_data, 0);
    return true;
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

        BOOST_FOREACH (const ImageIOParameter &p, m_spec.extra_params) {
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



void
IvImage::getpixel (int x, int y, int *pixel) const
{
    const OpenImageIO::stride_t as = OpenImageIO::AutoStride;
    OpenImageIO::convert_image (m_spec.nchannels, 1, 1, 1,
                                pixeladdr(x,y), m_spec.format, as, as, as,
                                pixel, PT_INT, as, as, as);
    std::cerr << "src offset " << pixeladdr(x,y) << ": " << pixel[0] << "\n";
}



void
IvImage::getpixel (int x, int y, float *pixel) const
{
    const OpenImageIO::stride_t as = OpenImageIO::AutoStride;
    OpenImageIO::convert_image (m_spec.nchannels, 1, 1, 1,
                                pixeladdr(x,y), m_spec.format, as, as, as,
                                pixel, PT_FLOAT, as, as, as);
}



int
IvImage::oriented_width () const
{
    return m_orientation <= 4 ? m_spec.width : m_spec.height;
}



int
IvImage::oriented_height () const
{
    return m_orientation <= 4 ? m_spec.height : m_spec.width;
}
