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

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;
#include <boost/foreach.hpp>
#include <boost/scoped_ptr.hpp>
#include <half.h>

#include "imageviewer.h"
#include "strutil.h"
#include "fmath.h"

namespace OIIO = OIIO_NAMESPACE;

IvImage::IvImage (const std::string &filename)
    : ImageBuf(filename), m_thumbnail(NULL),
      m_thumbnail_valid(false),
      m_gamma(1), m_exposure(0),
      m_file_dataformat(TypeDesc::UNKNOWN), 
      m_image_valid(false), m_auto_subimage(false)
{
}



IvImage::~IvImage ()
{
    delete [] m_thumbnail;
}



bool
IvImage::init_spec (const std::string &filename, int subimage, int miplevel)
{
    // invalidate info strings
    m_shortinfo.clear ();
    m_longinfo.clear ();

    // If we're changing mip levels or subimages, the pixels will no
    // longer be valid.
    if (subimage != m_current_subimage || miplevel != m_current_miplevel)
        m_image_valid = false;
    bool ok = ImageBuf::init_spec (filename, subimage, miplevel);
    if (ok && m_file_dataformat.basetype == TypeDesc::UNKNOWN) {
        m_file_dataformat = m_spec.format;
    }
    return ok;
}



bool
IvImage::read (int subimage, int miplevel, bool force, TypeDesc format,
               ProgressCallback progress_callback,
               void *progress_callback_data, bool secondary_data)
{
    // Don't read if we already have it in memory, unless force is true.
    // FIXME: should we also check the time on the file to see if it's
    // been updated since we last loaded?
    if (m_image_valid && !force
          && subimage == m_current_subimage && miplevel == m_current_miplevel)
        return true;

    // invalidate info strings
    m_shortinfo.clear();
    m_longinfo.clear();

    m_image_valid = ImageBuf::read (subimage, miplevel, force, format,
                              progress_callback, progress_callback_data);

    if (m_image_valid && secondary_data && m_spec.format == TypeDesc::UINT8) {
        m_corrected_image.reset ("", ImageSpec (m_spec.width, m_spec.height, std::min(m_spec.nchannels, 4), m_spec.format));
    } else {
        m_corrected_image.clear ();
    }
    return m_image_valid;
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



// Used by pixel_transform to convert from UINT8 to float.
static EightBitConverter<float> converter;



void 
IvImage::pixel_transform(bool srgb_to_linear, int color_mode, int select_channel)
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
    unsigned char correction_table[256];
    int total_channels = m_spec.nchannels;
    int color_channels = m_spec.nchannels;
    int max_channels = m_corrected_image.nchannels();

    // FIXME: Now with the iterator and data proxy in place, it should be
    // trivial to apply the transformations to any kind of data, not just
    // UINT8.
    if (m_spec.format != TypeDesc::UINT8 || ! m_corrected_image.localpixels()) {
        return;
    }

    if (color_channels > 3) {
        color_channels = 3;
    } else if (color_channels == 2) {
        color_channels = 1;
    }

    // This image is Luminance or Luminance + Alpha, and we are asked to show
    // luminance.
    if (color_channels == 1 && color_mode == 3) {
        color_mode = 0; // Just copy as usual.
    }

    // Happy path:
    if (! srgb_to_linear && color_mode <= 1 && m_gamma == 1.0 && m_exposure == 0.0) {
        ImageBuf::ConstIterator<unsigned char, unsigned char> src (*this);
        ImageBuf::Iterator<unsigned char, unsigned char> dst (m_corrected_image);
        for ( ; src.valid (); ++src) {
            dst.pos (src.x(), src.y());
            for (int i = 0; i < max_channels; i++)
                dst[i] = src[i];
        }
        return;
    }

    // fill the correction_table
    if (gamma() == 1.0 && exposure() == 0.0) {
        for (int pixelvalue = 0; pixelvalue < 256; ++pixelvalue) {
            correction_table[pixelvalue] = pixelvalue;
        }
    } else {
        float inv_gamma = 1.0/gamma();
        float gain = powf (2.0f, exposure());
        for (int pixelvalue = 0; pixelvalue < 256; ++pixelvalue) {
            float pv_f = converter (pixelvalue);
            pv_f = clamp (OIIO::exposure (pv_f, gain, inv_gamma),
                          0.0f, 1.0f);
            correction_table[pixelvalue] = (unsigned char) (pv_f*255 + 0.5);
        }
    }

    ImageBuf::ConstIterator<unsigned char, unsigned char> src (*this);
    ImageBuf::Iterator<unsigned char, unsigned char> dst (m_corrected_image);
    for ( ; src.valid(); ++src) {
        dst.pos (src.x(), src.y());
        if (color_mode == 0 || color_mode == 1) {
            // RGBA, RGB modes.
            int ch = 0;
            for (ch = 0; ch < color_channels; ch++) {
                if (srgb_to_linear)
                    dst[ch] = correction_table[srgb_to_linear_lut[src[ch]]];
                else
                    dst[ch] = correction_table[src[ch]];
            }
            for (; ch < max_channels; ch++) {
                dst[ch] = src[ch];
            }
        } else if (color_mode == 3) {
            // Convert RGB to luminance, (Rec. 709 luma coefficients).
            float luminance;
            if (srgb_to_linear) {
                luminance = converter (srgb_to_linear_lut[src[0]])*0.2126f +
                            converter (srgb_to_linear_lut[src[1]])*0.7152f +
                            converter (srgb_to_linear_lut[src[2]])*0.0722f;
            } else {
                luminance = converter (src[0])*0.2126f +
                            converter (src[1])*0.7152f +
                            converter (src[2])*0.0722f;
            }
            unsigned char val = (unsigned char) (clamp (luminance, 0.0f, 1.0f) * 255.0 + 0.5);
            val = correction_table[val];
            dst[0] = val;
            dst[1] = val;
            dst[2] = val;

            // Handle the rest of the channels
            for (int ch = 3; ch < total_channels; ++ch) {
                dst[ch] = src[ch];
            }
        } else { // Single channel, heatmap.
            unsigned char v = 0;
            if (select_channel < color_channels) {
                if (srgb_to_linear)
                    v = correction_table[srgb_to_linear_lut[src[select_channel]]];
                else
                    v = correction_table[src[select_channel]];
            } else if (select_channel < total_channels) {
                v = src[select_channel];
            }
            int ch = 0;
            for (; ch < color_channels; ++ch) {
                dst[ch] = v;
            }
            for (; ch < max_channels; ++ch) {
                dst[ch] = src[ch];
            }
        } 
    }
}



void
IvImage::invalidate ()
{ 
    m_pixels_valid = false;
    m_thumbnail_valid = false;
    m_image_valid = false;
    if (m_imagecache) {
        m_imagecache->invalidate (m_name);
    }
}



template<typename S, typename D>
static inline void 
copy_pixel_channels_ (const ImageBuf &buf, int xbegin, int xend,
              int ybegin, int yend, int chbegin, int chend, D *r)
{
    int w = (xend-xbegin);
    int nc = (chend-chbegin);
    for (ImageBuf::ConstIterator<S,D> p (buf, xbegin, xend, ybegin, yend);
         p.valid(); ++p) { 
        imagesize_t offset = ((p.y()-ybegin)*w + (p.x()-xbegin)) * nc;
        for (int c = 0;  c < nc;  ++c)
            r[offset+c] = p[chbegin+c];
    }
}



template<typename D>
static inline bool
copy_pixel_channels (const ImageBuf &buf, int xbegin, int xend, 
              int ybegin, int yend, int chbegin, int chend, D *r)
{
    // Caveat: serious hack here.  To avoid duplicating code, use a
    // #define.  Furthermore, exploit the CType<> template to construct
    // the right C data type for the given BASETYPE.
#define TYPECASE(B)                                                     \
    case B : copy_pixel_channels_<CType<B>::type,D>(buf, xbegin, xend, ybegin, yend, chbegin, chend, (D *)r); return true
    
    switch (buf.spec().format.basetype) {
        TYPECASE (TypeDesc::UINT8);
        TYPECASE (TypeDesc::INT8);
        TYPECASE (TypeDesc::UINT16);
        TYPECASE (TypeDesc::INT16);
        TYPECASE (TypeDesc::UINT);
        TYPECASE (TypeDesc::INT);
        TYPECASE (TypeDesc::HALF);
        TYPECASE (TypeDesc::FLOAT);
        TYPECASE (TypeDesc::DOUBLE);
    }
    return false;
#undef TYPECASE
}



bool
IvImage::copy_pixel_channels (int xbegin, int xend, int ybegin, int yend,
                       int chbegin, int chend, TypeDesc format, void *result) const
{
    if (chend > nchannels())
        return false;

    // Fancy method -- for each possible base type that the user
    // wants for a destination type, call a template specialization.
    switch (format.basetype) {
    case TypeDesc::UINT8 :
        ::copy_pixel_channels<unsigned char> (*this, xbegin, xend, ybegin, yend, chbegin, chend, (unsigned char *)result);
        break;
    case TypeDesc::INT8:
        ::copy_pixel_channels<char> (*this, xbegin, xend, ybegin, yend, chbegin, chend, (char *)result);
        break;
    case TypeDesc::UINT16 :
        ::copy_pixel_channels<unsigned short> (*this, xbegin, xend, ybegin, yend, chbegin, chend, (unsigned short *)result);
        break;
    case TypeDesc::INT16 :
        ::copy_pixel_channels<short> (*this, xbegin, xend, ybegin, yend, chbegin, chend, (short *)result);
        break;
    case TypeDesc::UINT :
        ::copy_pixel_channels<unsigned int> (*this, xbegin, xend, ybegin, yend, chbegin, chend, (unsigned int *)result);
        break;
    case TypeDesc::INT :
        ::copy_pixel_channels<int> (*this, xbegin, xend, ybegin, yend, chbegin, chend, (int *)result);
        break;
    case TypeDesc::HALF :
        ::copy_pixel_channels<half> (*this, xbegin, xend, ybegin, yend, chbegin, chend, (half *)result);
        break;
    case TypeDesc::FLOAT :
        ::copy_pixel_channels<float> (*this, xbegin, xend, ybegin, yend, chbegin, chend, (float *)result);
        break;
    case TypeDesc::DOUBLE :
        ::copy_pixel_channels<double> (*this, xbegin, xend, ybegin, yend, chbegin, chend, (double *)result);
        break;
    default:
        return false;
    }
    return true;
}

