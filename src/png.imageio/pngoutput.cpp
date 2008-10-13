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

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <time.h>

#include <png.h>

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;

#include "dassert.h"
#include "imageio.h"
#include "strutil.h"

using namespace OpenImageIO;



class PNGOutput : public ImageOutput {
public:
    PNGOutput ();
    virtual ~PNGOutput ();
    virtual const char * format_name (void) const { return "png"; }
    virtual bool supports (const std::string &feature) const {
        // Support nothing nonstandard
        return false;
    }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       bool append=false);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    png_structp m_png;                ///< PNG read structure pointer
    png_infop m_info;                 ///< PNG image info structure pointer
    std::vector<unsigned char> m_scratch;
    std::vector<png_text> m_pngtext;

    // Initialize private members to pre-opened state
    void init (void) {
        m_file = NULL;
        m_png = NULL;
        m_info = NULL;
        m_pngtext.clear ();
    }

    // Add a parameter to the output
    bool put_parameter (const std::string &name, TypeDesc type,
                        const void *data);

    void finish_image ();
};




// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT PNGOutput *png_output_imageio_create () { return new PNGOutput; }

// DLLEXPORT int imageio_version = IMAGEIO_VERSION;   // it's in pngoutput.cpp

DLLEXPORT const char * png_output_extensions[] = {
    "png", NULL
};

};



PNGOutput::PNGOutput ()
{
    init ();
}



PNGOutput::~PNGOutput ()
{
    // Close, if not already done.
    close ();
}



bool
PNGOutput::open (const std::string &name, const ImageSpec &userspec, bool append)
{
    close ();  // Close any already-opened file
    m_spec = userspec;  // Stash the spec

    // Check for things this format doesn't support
    if (m_spec.width < 1 || m_spec.height < 1) {
        error ("Image resolution must be at least 1x1, you asked for %d x %d",
               m_spec.width, m_spec.height);
        return false;
    }
    if (m_spec.depth < 1)
        m_spec.depth = 1;
    if (m_spec.depth > 1) {
        error ("%s does not support volume images (depth > 1)", format_name());
        return false;
    }

    int color_type;
    switch (m_spec.nchannels) {
    case 1 : color_type = PNG_COLOR_TYPE_GRAY; break;
    case 2 : color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
    case 3 : color_type = PNG_COLOR_TYPE_RGB; break;
    case 4 : color_type = PNG_COLOR_TYPE_RGB_ALPHA; break;
    default:
        error ("PNG only supports 1-4 channels, not %d", m_spec.nchannels);
        return false;
    }

    m_file = fopen (name.c_str(), "wb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    m_png = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (! m_png) {
        error ("Could not create PNG write structure");
        return false;
    }

    m_info = png_create_info_struct (m_png);
    if (! m_info) {
        close ();
        error ("Could not create PNG info structure");
        return false;
    }

    // Must call this setjmp in every function that does PNG writes
    if (setjmp (png_jmpbuf(m_png))) {
        close ();
        error ("PNG library error");
        return false;
    }

    png_init_io (m_png, m_file);
    png_set_compression_level (m_png, Z_BEST_COMPRESSION);

    // Force either 16 or 8 bit integers
    if (m_spec.format != TypeDesc::UINT16)
        m_spec.format = TypeDesc::UINT8;

    png_set_IHDR (m_png, m_info, m_spec.width, m_spec.height,
                  m_spec.format.size()*8, color_type, PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_set_oFFs (m_png, m_info, m_spec.x, m_spec.y, PNG_OFFSET_PIXEL);

    switch (m_spec.linearity) {
    case ImageSpec::UnknownLinearity :
        break;
    case ImageSpec::Linear :
        png_set_gAMA (m_png, m_info, 1.0);
        break;
    case ImageSpec::GammaCorrected :
        png_set_gAMA (m_png, m_info, m_spec.gamma);
        break;
    case ImageSpec::sRGB :
        png_set_sRGB_gAMA_and_cHRM (m_png, m_info, PNG_sRGB_INTENT_ABSOLUTE);
        break;
    }

    if (false && ! m_spec.find_attribute("DateTime")) {
        time_t now;
        time (&now);
        struct tm mytm;
        localtime_r (&now, &mytm);
        std::string date = Strutil::format ("%4d:%02d:%02d %2d:%02d:%02d",
                               mytm.tm_year+1900, mytm.tm_mon+1, mytm.tm_mday,
                               mytm.tm_hour, mytm.tm_min, mytm.tm_sec);
        m_spec.attribute ("DateTime", date);
    }

    ImageIOParameter *unit, *xres, *yres;
    const char *str;
    if ((unit = m_spec.find_attribute("ResolutionUnit", TypeDesc::STRING)) &&
        (xres = m_spec.find_attribute("XResolution", TypeDesc::FLOAT)) &&
        (yres = m_spec.find_attribute("YResolution", TypeDesc::FLOAT))) {
        const char *unitname = *(const char **)unit->data();
        const float x = *(const float *)xres->data();
        const float y = *(const float *)yres->data();
        int unittype = PNG_RESOLUTION_UNKNOWN;
        float scale = 1;
        if (! strcmp (unitname, "meter") || ! strcmp (unitname, "m"))
            unittype = PNG_RESOLUTION_METER;
        else if (! strcmp (unitname, "cm")) {
            unittype = PNG_RESOLUTION_METER;
            scale = 100;
        } else if (! strcmp (unitname, "inch") || ! strcmp (unitname, "in")) {
            unittype = PNG_RESOLUTION_METER;
            scale = 100.0/2.54;
        }
        png_set_pHYs (m_png, m_info, (png_uint_32)(x*scale),
                      (png_uint_32)(y*scale), unittype);
    }

    // Deal with all other params
    for (size_t p = 0;  p < m_spec.extra_attribs.size();  ++p)
        put_parameter (m_spec.extra_attribs[p].name().string(),
                       m_spec.extra_attribs[p].type(),
                       m_spec.extra_attribs[p].data());

    if (m_pngtext.size())
        png_set_text (m_png, m_info, &m_pngtext[0], m_pngtext.size());

    png_write_info (m_png, m_info);
    png_set_packing (m_png);   // Pack 1, 2, 4 bit into bytes

    return true;
}



bool
PNGOutput::put_parameter (const std::string &_name, TypeDesc type,
                           const void *data)
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
            png_set_tIME (m_png, m_info, &mod_time);
            return true;
        } else {
            return false;
        }
    }

#if 0
    if (iequals(name, "ResolutionUnit") && type == TypeDesc::STRING) {
        const char *s = *(char**)data;
        bool ok = true;
        if (! strcmp (s, "none"))
            PNGSetField (m_tif, PNGTAG_RESOLUTIONUNIT, RESUNIT_NONE);
        else if (! strcmp (s, "in") || ! strcmp (s, "inch"))
            PNGSetField (m_tif, PNGTAG_RESOLUTIONUNIT, RESUNIT_INCH);
        else if (! strcmp (s, "cm"))
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
        m_pngtext.push_back (t);
    }

    return false;
}



void
PNGOutput::finish_image ()
{
    // Must call this setjmp in every function that does PNG writes
    if (setjmp (png_jmpbuf(m_png))) {
        error ("PNG library error");
        return;
    }
    png_write_end (m_png, NULL);
}



bool
PNGOutput::close ()
{
    if (m_png && m_info) {
        finish_image ();
        png_destroy_write_struct (&m_png, &m_info);
        m_png = NULL;
        m_info = NULL;
    }

    if (m_file) {
        fclose (m_file);
        m_file = NULL;
    }

    init ();      // re-initialize
    return true;  // How can we fail?
}



bool
PNGOutput::write_scanline (int y, int z, TypeDesc format,
                            const void *data, stride_t xstride)
{
    // Must call this setjmp in every function that does PNG writes
    if (setjmp (png_jmpbuf (m_png))) {
        error ("PNG library error");
        return false;
    }

    y -= m_spec.y;
    m_spec.auto_stride (xstride, format, spec().nchannels);
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch);
    if (data == origdata) {
        m_scratch.assign ((unsigned char *)data,
                          (unsigned char *)data+m_spec.scanline_bytes());
        data = &m_scratch[0];
    }
    png_write_row (m_png, (png_byte *)data);

    return true;
}
