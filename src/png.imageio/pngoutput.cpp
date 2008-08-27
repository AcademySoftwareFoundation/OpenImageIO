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
    virtual bool supports (const char *feature) const {
        // Support nothing nonstandard
        return false;
    }
    virtual bool open (const char *name, const ImageSpec &spec,
                       bool append=false);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, ParamBaseType format,
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
    bool put_parameter (const std::string &name, ParamType type,
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
PNGOutput::open (const char *name, const ImageSpec &userspec, bool append)
{
    close ();  // Close any already-opened file
    m_spec = userspec;  // Stash the spec

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

    m_file = fopen (name, "wb");
    if (! m_file) {
        error ("Could not open file");
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
    if (m_spec.format != PT_UINT16)
        m_spec.format = PT_UINT8;

    png_set_IHDR (m_png, m_info, m_spec.width, m_spec.height,
                  typesize(m_spec.format)*8, color_type, PNG_INTERLACE_NONE,
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
    if ((unit = m_spec.find_attribute("ResolutionUnit", PT_STRING)) &&
        (xres = m_spec.find_attribute("XResolution", PT_FLOAT)) &&
        (yres = m_spec.find_attribute("YResolution", PT_FLOAT))) {
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
PNGOutput::put_parameter (const std::string &_name, ParamType type,
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
    if (iequals(name, "Artist") && type == PT_STRING)
        name = "Author";
    if ((iequals(name, "name") || iequals(name, "DocumentName")) &&
          type == PT_STRING)
        name = "Title";
    if ((iequals(name, "description") || iequals(name, "ImageDescription")) &&
          type == PT_STRING)
        name = "Description";

    if (iequals(name, "DateTime") && type == PT_STRING) {
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
    if (iequals(name, "ResolutionUnit") && type == PT_STRING) {
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
    if (iequals(name, "ResolutionUnit") && type == PT_UINT) {
        PNGSetField (m_tif, PNGTAG_RESOLUTIONUNIT, *(unsigned int *)data);
        return true;
    }
    if (iequals(name, "XResolution") && type == PT_FLOAT) {
        PNGSetField (m_tif, PNGTAG_XRESOLUTION, *(float *)data);
        return true;
    }
    if (iequals(name, "YResolution") && type == PT_FLOAT) {
        PNGSetField (m_tif, PNGTAG_YRESOLUTION, *(float *)data);
        return true;
    }
#endif
    if (type == PT_STRING) {
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
PNGOutput::write_scanline (int y, int z, ParamBaseType format,
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
