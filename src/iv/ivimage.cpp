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
#include "fmath.h"



IvImage::IvImage (const std::string &filename)
    : ImageBuf(filename), m_thumbnail(NULL),
      m_thumbnail_valid(false),
      m_gamma(1), m_exposure(0),
      m_file_dataformat(TypeDesc::UNKNOWN)
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

    bool ok = ImageBuf::init_spec (filename);
    if (ok && m_file_dataformat.basetype == TypeDesc::UNKNOWN) {
        m_file_dataformat = m_spec.format;
    }
    return ok;
}



bool
IvImage::read (int subimage, bool force, TypeDesc format,
               OpenImageIO::ProgressCallback progress_callback,
               void *progress_callback_data, bool secondary_data)
{
    // Don't read if we already have it in memory, unless force is true.
    // FIXME: should we also check the time on the file to see if it's
    // been updated since we last loaded?
    if (m_pixels.size() && m_pixels_valid && !force && subimage == m_current_subimage)
        return true;

    // invalidate info strings
    m_shortinfo.clear();
    m_longinfo.clear();

    bool ok = ImageBuf::read (subimage, force, format,
                              progress_callback, progress_callback_data);

    if (ok && secondary_data && m_spec.format == TypeDesc::UINT8) {
        m_secondary.resize (m_spec.image_bytes ());
    } else {
        m_secondary.clear ();
    }
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
                                        m_file_dataformat.c_str(),
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
        m_longinfo += html_table_row ("Data format", m_file_dataformat.c_str());
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
            std::string s = m_spec.metadata_val (p, true);
            m_longinfo += html_table_row (p.name().c_str(), s);
        }

        m_longinfo += "</table>";
    }
    return m_longinfo;
}



void
IvImage::srgb_to_linear ()
{
    /// This table obeys the following function:
    ///
    ///   unsigned char srgb2linear(unsigned char x)
    ///   {
    ///       float x_f = x/255.0;
    ///       float x_l = 0.0;
    ///       if (x_f <= 0.04045)
    ///           x_l = x_f/12.92;
    ///       else
    ///           x_l = powf((x_f+0.055)/1.055,2.4);
    ///       return (unsigned char)(x_l * 255 + 0.5)
    ///   }
    /// 
    ///  It's used to transform from sRGB color space to linear color space.
    static const unsigned char srgb_to_linear_lut[256] = {
        0, 0, 0, 0, 0, 0, 0, 1,
        1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 2, 2, 2, 2, 2, 2,
        2, 2, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 5, 5, 5,
        5, 6, 6, 6, 6, 7, 7, 7,
        8, 8, 8, 8, 9, 9, 9, 10,
        10, 10, 11, 11, 12, 12, 12, 13,
        13, 13, 14, 14, 15, 15, 16, 16,
        17, 17, 17, 18, 18, 19, 19, 20,
        20, 21, 22, 22, 23, 23, 24, 24,
        25, 25, 26, 27, 27, 28, 29, 29,
        30, 30, 31, 32, 32, 33, 34, 35,
        35, 36, 37, 37, 38, 39, 40, 41,
        41, 42, 43, 44, 45, 45, 46, 47,
        48, 49, 50, 51, 51, 52, 53, 54,
        55, 56, 57, 58, 59, 60, 61, 62,
        63, 64, 65, 66, 67, 68, 69, 70,
        71, 72, 73, 74, 76, 77, 78, 79,
        80, 81, 82, 84, 85, 86, 87, 88,
        90, 91, 92, 93, 95, 96, 97, 99,
        100, 101, 103, 104, 105, 107, 108, 109,
        111, 112, 114, 115, 116, 118, 119, 121,
        122, 124, 125, 127, 128, 130, 131, 133,
        134, 136, 138, 139, 141, 142, 144, 146,
        147, 149, 151, 152, 154, 156, 157, 159,
        161, 163, 164, 166, 168, 170, 171, 173,
        175, 177, 179, 181, 183, 184, 186, 188,
        190, 192, 194, 196, 198, 200, 202, 204,
        206, 208, 210, 212, 214, 216, 218, 220,
        222, 224, 226, 229, 231, 233, 235, 237,
        239, 242, 244, 246, 248, 250, 253, 255
    };

    if (m_spec.format != TypeDesc::UINT8 || m_spec.linearity != ImageSpec::sRGB) {
        return;
    }

    int total_channels = m_spec.nchannels;
    // Number of channels which represent color (and must be transformed to
    // linear)
    int color_channels = total_channels;
    // Interpret two channels as luminance + alpha.
    if (total_channels == 2) {
        color_channels = 1;
    } else if (color_channels > 3) {
        color_channels = 3;
    }

    for (int y = 0; y <= ymax(); ++y) {
        unsigned char *sl = (unsigned char*)ImageBuf::scanline(y);
        for (int x = 0; x <= xmax(); ++x) {
            for (int ch = 0; ch < color_channels; ++ch) {
                unsigned char srgb = sl[x*total_channels + ch];
                unsigned char linear = srgb_to_linear_lut[srgb];
                sl[x*total_channels + ch] = linear;
            }
        }
    }
}



// Used by apply_corrections and select_channel to convert from UINT8 to float.
static EightBitConverter<float> converter;



void
IvImage::apply_corrections ()
{
    // FIXME: Merge select_channel and this method together.

    unsigned char correction_table[256];

    if (m_spec.format != TypeDesc::UINT8 || m_secondary.empty()) {
        return;
    }

    float inv_gamma = 1.0/gamma();
    float gain = powf (2.0, exposure());

    // first, fill the correction table.
    for (int pixelvalue = 0; pixelvalue < 256; ++pixelvalue) {
        float pv_f = converter (pixelvalue);
        float corrected = clamp (OpenImageIO::exposure (pv_f, gain, inv_gamma),
                                 0.0f, 1.0f);
        correction_table[pixelvalue] = (unsigned char)RoundToInt (corrected*255);
    }

    int total_channels = m_spec.nchannels;
    // Number of channels which represent color (and must have its exposure
    // corrected).
    int color_channels = total_channels;
    if (total_channels == 2) {
        // Two channels is lum+alpha.
        color_channels = 1;
    } else if (color_channels > 3) {
        color_channels = 3;
    }

    for (int y = 0; y <= ymax(); ++y) {
        unsigned char *sl = &m_secondary[y * m_spec.scanline_bytes()];
        for (int x = 0; x <= xmax(); ++x) {
            for (int ch = 0; ch < color_channels; ++ch) {
                unsigned char value = sl[x*total_channels + ch];
                unsigned char corrected = correction_table[value];
                sl[x*total_channels + ch] = corrected;
            }
        }
    }
}



void
IvImage::select_channel (int channel)
{
    int total_channels = m_spec.nchannels;
    int color_channels = total_channels;
    if (color_channels > 3) {
        color_channels = 3;
    }
    if (m_spec.format.basetype != TypeDesc::UINT8 || m_secondary.empty()) {
        return;
    }

    // Show RGB(A) in its whole glory.
    if (channel == -1 || total_channels == 1) {
        m_secondary.assign (m_pixels.begin(), m_pixels.end());
        return;
    }
    if (channel == -2 && total_channels < 3) {
        // does this makes sense?
        // This is an attempt to convert a 2 channel image to luminance, but
        // 2 channel image is interpreted as luminance+alpha.
        m_secondary.assign (m_pixels.begin(), m_pixels.end());
        return;
    }

    for (int y = 0; y <= ymax(); ++y) {
        unsigned char *sl_src = (unsigned char*) ImageBuf::scanline (y);
        unsigned char *sl_dest = &m_secondary[y * m_spec.scanline_bytes()];
        for (int x = 0; x <= xmax(); ++x) {
            if (channel == -2) {
                // Convert RGB to luminance, (Rec. 709 luma coefficients).
                float f_lum = converter (sl_src[x*total_channels + 0])*0.2126f +
                              converter (sl_src[x*total_channels + 1])*0.7152f +
                              converter (sl_src[x*total_channels + 2])*0.0722f;
                unsigned char lum = (unsigned char)RoundToInt (clamp (f_lum, 0.0f, 1.0f) * 255.0);
                sl_dest[x*total_channels + 0] = lum;
                sl_dest[x*total_channels + 1] = lum;
                sl_dest[x*total_channels + 2] = lum;

                // Handle the rest of the channels
                for (int ch = color_channels; ch < total_channels; ++ch) {
                    sl_dest[x*total_channels + ch] = sl_src[x*total_channels + ch];
                }
            } else {
                unsigned char v = 0;
                if (channel < total_channels) {
                    v = sl_src[x*total_channels + channel];
                } else {
                    // This at least makes sense for the alpha channel when
                    // there are no alpha values.
                    v = 255;
                }
                int ch = 0;
                for (; ch < color_channels; ++ch) {
                    sl_dest[x*total_channels + ch] = v;
                }
                for (; ch < total_channels; ++ch) {
                    sl_dest[x*total_channels + ch] = sl_src[x*total_channels + ch];
                }
            }
        }
    }
}
