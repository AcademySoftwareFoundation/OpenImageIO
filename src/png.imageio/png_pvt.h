/*
  Copyright 2009 Larry Gritz and the other authors and contributors.
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

#ifndef OPENIMAGEIO_PNG_PVT_H
#define OPENIMAGEIO_PNG_PVT_H

#include <png.h>

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;
#include <OpenEXR/ImathColor.h>

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "strutil.h"
#include "fmath.h"
#include "sysutil.h"



/*
This code has been extracted from the PNG plugin so as to provide access to PNG
images embedded within any container format, without redundant code
duplication.

It's been done in the course of development of the ICO plugin in order to allow
reading and writing Vista-style PNG icons.

For further information see the following mailing list threads:
http://lists.openimageio.org/pipermail/oiio-dev-openimageio.org/2009-April/000586.html
http://lists.openimageio.org/pipermail/oiio-dev-openimageio.org/2009-April/000656.html
*/

OIIO_PLUGIN_NAMESPACE_BEGIN

namespace PNG_pvt {

/// Initializes a PNG read struct.
/// \return empty string on success, error message on failure.
///
inline const std::string
create_read_struct (png_structp& sp, png_infop& ip)
{
    sp = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (! sp)
        return "Could not create PNG read structure";

    ip = png_create_info_struct (sp);
    if (! ip)
        return "Could not create PNG info structure";

    // Must call this setjmp in every function that does PNG reads
    if (setjmp (png_jmpbuf(sp)))
        return "PNG library error";

    // success
    return "";
}



/// Helper function - reads background colour.
///
inline bool
get_background (png_structp& sp, png_infop& ip, ImageSpec& spec,
                int& bit_depth, float *red, float *green, float *blue)
{
    if (setjmp (png_jmpbuf (sp)))
        return false;
    if (! png_get_valid (sp, ip, PNG_INFO_bKGD))
        return false;

    png_color_16p bg;
    png_get_bKGD (sp, ip, &bg);
    if (spec.format == TypeDesc::UINT16) {
        *red   = bg->red   / 65535.0;
        *green = bg->green / 65535.0;
        *blue  = bg->blue  / 65535.0;
    } else if (spec.nchannels < 3 && bit_depth < 8) {
        if (bit_depth == 1)
            *red = *green = *blue = (bg->gray ? 1 : 0);
        else if (bit_depth == 2)
            *red = *green = *blue = bg->gray / 3.0;
        else // 4 bits
            *red = *green = *blue = bg->gray / 15.0;
    } else {
        *red   = bg->red   / 255.0;
        *green = bg->green / 255.0;
        *blue  = bg->blue  / 255.0;
    }
    return true;
}



/// Read information from a PNG file and fill the ImageSpec accordingly.
///
inline void
read_info (png_structp& sp, png_infop& ip, int& bit_depth, int& color_type,
           int& interlace_type,
           Imath::Color3f& bg, ImageSpec& spec)
{
    png_read_info (sp, ip);

    // Auto-convert 1-, 2-, and 4- bit images to 8 bits, palette to RGB,
    // and transparency to alpha.
    png_set_expand (sp);

    // PNG files are naturally big-endian
    if (littleendian())
        png_set_swap (sp);

    png_read_update_info (sp, ip);
    
    png_uint_32 width, height;
    png_get_IHDR (sp, ip, &width, &height,
                  &bit_depth, &color_type, NULL, NULL, NULL);
    

    spec = ImageSpec ((int)width, (int)height, 
                       png_get_channels (sp, ip),
                       bit_depth == 16 ? TypeDesc::UINT16 : TypeDesc::UINT8);

    spec.default_channel_names ();

    double gamma;
    if (png_get_gAMA (sp, ip, &gamma)) {
        spec.attribute ("oiio:Gamma", (float) gamma);
        spec.attribute ("oiio:ColorSpace", (gamma == 1) ? "Linear" : "GammaCorrected");
    }
    int srgb_intent;
    if (png_get_sRGB (sp, ip, &srgb_intent)) {
        spec.attribute ("oiio:ColorSpace", "sRGB");
    }
    png_timep mod_time;
    if (png_get_tIME (sp, ip, &mod_time)) {
        std::string date = Strutil::format ("%4d:%02d:%02d %2d:%02d:%02d",
                           mod_time->year, mod_time->month, mod_time->day,
                           mod_time->hour, mod_time->minute, mod_time->second);
        spec.attribute ("DateTime", date); 
    }
    
    png_textp text_ptr;
    int num_comments = png_get_text (sp, ip, &text_ptr, NULL);
    if (num_comments) {
        std::string comments;
        for (int i = 0;  i < num_comments;  ++i) {
            if (iequals (text_ptr[i].key, "Description"))
                spec.attribute ("ImageDescription", text_ptr[i].text);
            else if (iequals (text_ptr[i].key, "Author"))
                spec.attribute ("Artist", text_ptr[i].text);
            else if (iequals (text_ptr[i].key, "Title"))
                spec.attribute ("DocumentName", text_ptr[i].text);
            else
                spec.attribute (text_ptr[i].key, text_ptr[i].text);
        }
    }
    spec.x = png_get_x_offset_pixels (sp, ip);
    spec.y = png_get_y_offset_pixels (sp, ip);

    int unit;
    png_uint_32 resx, resy;
    if (png_get_pHYs (sp, ip, &resx, &resy, &unit)) {
        float scale = 1;
        if (unit == PNG_RESOLUTION_METER) {
            // Convert to inches, to match most other formats
            scale = 2.54 / 100.0;
            spec.attribute ("ResolutionUnit", "inch");
        } else
            spec.attribute ("ResolutionUnit", "none");
        spec.attribute ("XResolution", (float)resx*scale);
        spec.attribute ("YResolution", (float)resy*scale);
    }

    float aspect = (float)png_get_pixel_aspect_ratio (sp, ip);
    if (aspect != 0 && aspect != 1)
        spec.attribute ("PixelAspectRatio", aspect);

    float r, g, b;
    if (get_background (sp, ip, spec, bit_depth, &r, &g, &b)) {
        bg = Imath::Color3f (r, g, b);
        // FIXME -- should we do anything with the background color?
    }

    interlace_type = png_get_interlace_type (sp, ip);

    // PNG files are always "unassociated alpha"
    spec.attribute ("oiio:UnassociatedAlpha", (int)1);

    // FIXME -- look for an XMP packet in an iTXt chunk.
}



/// Reads from an open PNG file into the indicated buffer.
/// \return empty string on success, error message on failure.
///
inline const std::string
read_into_buffer (png_structp& sp, png_infop& ip, ImageSpec& spec,
                  int& bit_depth, int& color_type, std::vector<unsigned char>& buffer)
{
    // Must call this setjmp in every function that does PNG reads
    if (setjmp (png_jmpbuf (sp)))
        return "PNG library error";

#if 0
    // ?? This doesn't seem necessary, but I don't know why
    // Make the library handle fewer significant bits
    // png_color_8p sig_bit;
    // if (png_get_sBIT (sp, ip, &sig_bit)) {
    //        png_set_shift (sp, sig_bit);
    // }
#endif

    DASSERT (spec.scanline_bytes() == png_get_rowbytes(sp,ip));
    buffer.resize (spec.image_bytes());

    std::vector<unsigned char *> row_pointers (spec.height);
    for (int i = 0;  i < spec.height;  ++i)
        row_pointers[i] = &buffer[0] + i * spec.scanline_bytes();

    png_read_image (sp, &row_pointers[0]);
    png_read_end (sp, NULL);

    // success
    return "";
}



/// Reads the next scanline from an open PNG file into the indicated buffer.
/// \return empty string on success, error message on failure.
///
inline const std::string
read_next_scanline (png_structp& sp, void *buffer)
{
    // Must call this setjmp in every function that does PNG reads
    if (setjmp (png_jmpbuf (sp)))
        return "PNG library error";

    png_read_row (sp, (png_bytep)buffer, NULL);

    // success
    return "";
}



/// Destroys a PNG read struct.
///
inline void
destroy_read_struct (png_structp& sp, png_infop& ip)
{
    if (sp && ip) {
        png_destroy_read_struct (&sp, &ip, NULL);
        sp = NULL;
        ip = NULL;
    }
}



/// Initializes a PNG write struct.
/// \return empty string on success, C-string error message on failure.
///
inline const std::string
create_write_struct (png_structp& sp, png_infop& ip, int& color_type,
                     ImageSpec& spec)
{
    // Check for things this format doesn't support
    if (spec.width < 1 || spec.height < 1)
        return Strutil::format ("Image resolution must be at least 1x1, "
                               "you asked for %d x %d",
                               spec.width, spec.height);
    if (spec.depth < 1)
        spec.depth = 1;
    if (spec.depth > 1)
        return "PNG does not support volume images (depth > 1)";

    switch (spec.nchannels) {
    case 1 : color_type = PNG_COLOR_TYPE_GRAY; break;
    case 2 : color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
    case 3 : color_type = PNG_COLOR_TYPE_RGB; break;
    case 4 : color_type = PNG_COLOR_TYPE_RGB_ALPHA; break;
    default:
        return Strutil::format  ("PNG only supports 1-4 channels, not %d",
                                 spec.nchannels);
    }

    sp = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (! sp)
        return "Could not create PNG write structure";

    ip = png_create_info_struct (sp);
    if (! ip)
        return "Could not create PNG info structure";

    // Must call this setjmp in every function that does PNG writes
    if (setjmp (png_jmpbuf(sp)))
        return "PNG library error";

    // success
    return "";
}



/// Helper function - writes a single parameter.
///
inline bool
put_parameter (png_structp& sp, png_infop& ip, const std::string &_name,
               TypeDesc type, const void *data, std::vector<png_text>& text)
{
    std::string name = _name;

    // Things to skip
    if (iequals(name, "planarconfig"))  // No choice for PNG files
        return false;
    if (iequals(name, "compression"))
        return false;
    if (iequals(name, "ResolutionUnit") ||
          iequals(name, "XResolution") || iequals(name, "YResolution"))
        return false;

    // Remap some names to PNG conventions
    if (iequals(name, "Artist") && type == TypeDesc::STRING)
        name = "Author";
    if ((iequals(name, "name") || iequals(name, "DocumentName")) &&
          type == TypeDesc::STRING)
        name = "Title";
    if ((iequals(name, "description") || iequals(name, "ImageDescription")) &&
          type == TypeDesc::STRING)
        name = "Description";

    if (iequals(name, "DateTime") && type == TypeDesc::STRING) {
        png_time mod_time;
        int year, month, day, hour, minute, second;
        if (sscanf (*(const char **)data, "%4d:%02d:%02d %2d:%02d:%02d",
                    &year, &month, &day, &hour, &minute, &second) == 6) {
            mod_time.year = year;
            mod_time.month = month;
            mod_time.day = day;
            mod_time.hour = hour;
            mod_time.minute = minute;
            mod_time.second = second;
            png_set_tIME (sp, ip, &mod_time);
            return true;
        } else {
            return false;
        }
    }

#if 0
    if (iequals(name, "ResolutionUnit") && type == TypeDesc::STRING) {
        const char *s = *(char**)data;
        bool ok = true;
        if (iequals (s, "none"))
            PNGSetField (m_tif, PNGTAG_RESOLUTIONUNIT, RESUNIT_NONE);
        else if (iequals (s, "in") || iequals (s, "inch"))
            PNGSetField (m_tif, PNGTAG_RESOLUTIONUNIT, RESUNIT_INCH);
        else if (iequals (s, "cm"))
            PNGSetField (m_tif, PNGTAG_RESOLUTIONUNIT, RESUNIT_CENTIMETER);
        else ok = false;
        return ok;
    }
    if (iequals(name, "ResolutionUnit") && type == TypeDesc::UINT) {
        PNGSetField (m_tif, PNGTAG_RESOLUTIONUNIT, *(unsigned int *)data);
        return true;
    }
    if (iequals(name, "XResolution") && type == TypeDesc::FLOAT) {
        PNGSetField (m_tif, PNGTAG_XRESOLUTION, *(float *)data);
        return true;
    }
    if (iequals(name, "YResolution") && type == TypeDesc::FLOAT) {
        PNGSetField (m_tif, PNGTAG_YRESOLUTION, *(float *)data);
        return true;
    }
#endif
    if (type == TypeDesc::STRING) {
        png_text t;
        t.compression = PNG_TEXT_COMPRESSION_NONE;
        t.key = (char *)ustring(name).c_str();
        t.text = *(char **)data;   // Already uniquified
        text.push_back (t);
    }

    return false;
}



/// Writes PNG header according to the ImageSpec.
///
inline void
write_info (png_structp& sp, png_infop& ip, int& color_type,
            ImageSpec& spec, std::vector<png_text>& text)
{
    // Force either 16 or 8 bit integers
    if (spec.format == TypeDesc::UINT8 || spec.format == TypeDesc::INT8)
        spec.set_format (TypeDesc::UINT8);
    else
        spec.set_format (TypeDesc::UINT16); // best precision available

    png_set_IHDR (sp, ip, spec.width, spec.height,
                  spec.format.size()*8, color_type, PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_set_oFFs (sp, ip, spec.x, spec.y, PNG_OFFSET_PIXEL);

    std::string colorspace = spec.get_string_attribute ("oiio:ColorSpace");
    if (iequals (colorspace, "Linear")) {
        png_set_gAMA (sp, ip, 1.0);
    }
    else if (iequals (colorspace, "GammaCorrected")) {
        float gamma = spec.get_float_attribute ("oiio:Gamma", 1.0);
        png_set_gAMA (sp, ip, gamma);
    }
    else if (iequals (colorspace, "sRGB")) {
        png_set_sRGB_gAMA_and_cHRM (sp, ip, PNG_sRGB_INTENT_ABSOLUTE);
    }
    
    if (false && ! spec.find_attribute("DateTime")) {
        time_t now;
        time (&now);
        struct tm mytm;
        Sysutil::get_local_time (&now, &mytm);
        std::string date = Strutil::format ("%4d:%02d:%02d %2d:%02d:%02d",
                               mytm.tm_year+1900, mytm.tm_mon+1, mytm.tm_mday,
                               mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
        spec.attribute ("DateTime", date);
    }

    ImageIOParameter *unit=NULL, *xres=NULL, *yres=NULL;
    if ((unit = spec.find_attribute("ResolutionUnit", TypeDesc::STRING)) &&
        (xres = spec.find_attribute("XResolution", TypeDesc::FLOAT)) &&
        (yres = spec.find_attribute("YResolution", TypeDesc::FLOAT))) {
        const char *unitname = *(const char **)unit->data();
        const float x = *(const float *)xres->data();
        const float y = *(const float *)yres->data();
        int unittype = PNG_RESOLUTION_UNKNOWN;
        float scale = 1;
        if (iequals (unitname, "meter") || iequals (unitname, "m"))
            unittype = PNG_RESOLUTION_METER;
        else if (iequals (unitname, "cm")) {
            unittype = PNG_RESOLUTION_METER;
            scale = 100;
        } else if (iequals (unitname, "inch") || iequals (unitname, "in")) {
            unittype = PNG_RESOLUTION_METER;
            scale = 100.0/2.54;
        }
        png_set_pHYs (sp, ip, (png_uint_32)(x*scale),
                      (png_uint_32)(y*scale), unittype);
    }

    // Deal with all other params
    for (size_t p = 0;  p < spec.extra_attribs.size();  ++p)
        put_parameter (sp, ip,
                       spec.extra_attribs[p].name().string(),
                       spec.extra_attribs[p].type(),
                       spec.extra_attribs[p].data(),
                       text);

    if (text.size())
        png_set_text (sp, ip, &text[0], text.size());

    png_write_info (sp, ip);
    png_set_packing (sp);   // Pack 1, 2, 4 bit into bytes
}



/// Writes a scanline.
///
inline bool
write_row (png_structp& sp, png_byte *data)
{
    if (setjmp (png_jmpbuf(sp))) {
        //error ("PNG library error");
        return false;
    }
    png_write_row (sp, data);
    return true;
}



/// Helper function - finalizes writing the image.
///
inline void
finish_image (png_structp& sp)
{
    // Must call this setjmp in every function that does PNG writes
    if (setjmp (png_jmpbuf(sp))) {
        //error ("PNG library error");
        return;
    }
    png_write_end (sp, NULL);
}



/// Destroys a PNG write struct.
///
inline void
destroy_write_struct (png_structp& sp, png_infop& ip)
{
    if (sp && ip) {
        finish_image (sp);
        png_destroy_write_struct (&sp, &ip);
        sp = NULL;
        ip = NULL;
    }
}



};  // namespace PNG_pvt

OIIO_PLUGIN_NAMESPACE_END

#endif  // OPENIMAGEIO_PNG_PVT_H
