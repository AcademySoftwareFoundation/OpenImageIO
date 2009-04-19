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

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "ico.h"
using namespace ICO_pvt;

#include <png.h>

#include <boost/algorithm/string.hpp>
using boost::algorithm::iequals;

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "thread.h"
#include "strutil.h"
#include "fmath.h"

using namespace OpenImageIO;

class ICOOutput : public ImageOutput {
public:
    ICOOutput ();
    virtual ~ICOOutput ();
    virtual const char * format_name (void) const { return "ico"; }
    virtual bool supports (const std::string &feature) const;
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       bool append=false);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    int m_colour_type;                ///< Requested colour type
    bool m_want_png;                  ///< Whether the client requested PNG
    std::vector<unsigned char> m_scratch; ///< Scratch buffer
    int m_offset;                     ///< Offset to subimage data chunk
    int m_xor_slb;                    ///< XOR mask scanline length in bytes
    int m_and_slb;                    ///< AND mask scanline length in bytes
    int m_bpp;                        ///< Bits per pixel

    png_structp m_png;                ///< PNG read structure pointer
    png_infop m_info;                 ///< PNG image info structure pointer
    std::vector<png_text> m_pngtext;
    
    /// Initialize private members to pre-opened state
    void init (void) {
        m_file = NULL;
        m_png = NULL;
        m_info = NULL;
        m_pngtext.clear ();
    }

    /// Add a parameter to the output
    bool put_parameter (const std::string &name, TypeDesc type,
                        const void *data);

    /// Finish the writing of a PNG subimage
    void finish_png_image ();
};




// Obligatory material to make this a recognizeable imageio plugin:
extern "C" {

DLLEXPORT ImageOutput *ico_output_imageio_create () { return new ICOOutput; }

// DLLEXPORT int ico_imageio_version = OPENIMAGEIO_PLUGIN_VERSION;   // it's in icoinput.cpp

DLLEXPORT const char * ico_output_extensions[] = {
    "ico", NULL
};

};



ICOOutput::ICOOutput ()
{
    init ();
}



ICOOutput::~ICOOutput ()
{
    // Close, if not already done.
    close ();
}



bool
ICOOutput::open (const std::string &name, const ImageSpec &userspec, bool append)
{
    close ();  // Close any already-opened file
    m_spec = userspec;  // Stash the spec

    // Check for things this format doesn't support
    if (m_spec.width < 1 || m_spec.height < 1) {
        error ("Image resolution must be at least 1x1, you asked for %d x %d",
               m_spec.width, m_spec.height);
        return false;
    } else if (m_spec.width > 256 || m_spec.height > 256) {
        error ("Image resolution must be at most 256x256, you asked for %d x %d",
               m_spec.width, m_spec.height);
        return false;
    }
    if (m_spec.depth < 1)
        m_spec.depth = 1;
    if (m_spec.depth > 1) {
        error ("%s does not support volume images (depth > 1)", format_name());
        return false;
    }

    // reuse PNG constants for DIBs as well
    switch (m_spec.nchannels) {
    case 1 : m_colour_type = PNG_COLOR_TYPE_GRAY; break;
    case 2 : m_colour_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
    case 3 : m_colour_type = PNG_COLOR_TYPE_RGB; break;
    case 4 : m_colour_type = PNG_COLOR_TYPE_RGB_ALPHA; break;
    default:
        error ("ICO only supports 1-4 channels, not %d", m_spec.nchannels);
        return false;
    }

    m_bpp = (m_colour_type == PNG_COLOR_TYPE_GRAY_ALPHA
            || m_colour_type == PNG_COLOR_TYPE_RGB_ALPHA) ? 32 : 24;
    m_xor_slb = (m_spec.width * m_bpp + 7) / 8 // real data bytes
                + (4 - ((m_spec.width * m_bpp + 7) / 8) % 4) % 4; // padding
    m_and_slb = (m_spec.width + 7) / 8 // real data bytes
                + (4 - ((m_spec.width + 7) / 8) % 4) % 4; // padding

    // Force either 16 or 8 bit integers
    if (m_spec.format != TypeDesc::UINT16)
        m_spec.format = TypeDesc::UINT8;

    //std::cerr << "[ico] writing at " << m_bpp << "bpp\n";

    m_file = fopen (name.c_str(), append ? "a+b" : "wb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    // check if the client wants this subimage written as PNG
    const ImageIOParameter *p = m_spec.find_attribute ("ico:PNG",
                                                       TypeDesc::TypeInt);
    m_want_png = p && *(int *)p->data();

    ico_header ico;
    if (!append) {
        // creating new file, write ICO header
        memset (&ico, 0, sizeof(ico));
        ico.type = 1;
        ico.count = 1;
        fwrite (&ico, 1, sizeof(ico), m_file);
        m_offset = sizeof(ico_header) + sizeof(ico_subimage);
    } else {
        // we'll be appending data, so see what's already in the file
        fseek (m_file, 0, SEEK_END);
        fread (&ico, 1, sizeof(ico), m_file);
        if (bigendian()) {
            // ICOs are little endian
            swap_endian (&ico.type);
            swap_endian (&ico.count);
        }
        if (ico.reserved != 0 || ico.type != 1) {
            error ("File failed ICO header check");
            return false;
        }

        // need to move stuff around to make room for another subimage header
        int subimage = ico.count++;
        fseek (m_file, 0, SEEK_END);
        int len = ftell (m_file);
        unsigned char buf[512];
        // append null data at the end of file so that we don't seek beyond eof
        fwrite (buf, sizeof (ico_subimage), 1, m_file);

        // do the actual moving, 0.5kB per iteration
        int amount;
        for (int left = len - sizeof (ico_header) - sizeof (ico_subimage)
             * (subimage - 1); left > 0; left -= sizeof (buf)) {
            amount = left < sizeof (buf) ? len % sizeof (buf) : sizeof (buf);
            fseek (m_file, len - amount, SEEK_SET);
            fread (buf, amount, 1, m_file);
            fseek (m_file, len - amount + sizeof (ico_subimage), SEEK_SET);
            fwrite (buf, amount, 1, m_file);
        }
        // update header
        fseek (m_file, 0, SEEK_SET);
        // swap these back to little endian, if needed
        if (bigendian()) {
            swap_endian (&ico.type);
            swap_endian (&ico.count);
        }
        fwrite (&ico, sizeof (ico), 1, m_file);

        // and finally, update the offsets in subimage headers to point to
        // their data correctly
        uint32_t temp;
        fseek (m_file, offsetof (ico_subimage, ofs), SEEK_CUR);
        for (int i = 0; i < subimage; i++) {
            fread (&temp, sizeof (temp), 1, m_file);
            if (bigendian())
                swap_endian (&temp);
            temp += sizeof (ico_subimage);
            if (bigendian())
                swap_endian (&temp);
            // roll back 4 bytes, we need to rewrite the value we just read
            fseek (m_file, -4, SEEK_CUR);
            fwrite (&temp, sizeof (temp), 1, m_file);
            // skip to the next subimage; subtract 4 bytes because that's how
            // much we've just written
            fseek (m_file, sizeof (ico_subimage) - 4, SEEK_CUR);
        }

        // offset at which we'll be writing new image data
        m_offset = len + sizeof (ico_subimage);

        // next part of code expects the file pointer to be where the new
        // subimage header is to be written
        fseek (m_file, sizeof (ico_header) + subimage * sizeof (ico_subimage),
                                                                    SEEK_SET);
    }

    // write subimage header
    ico_subimage subimg;
    memset (&subimg, 0, sizeof(subimg));
    subimg.width = m_spec.width;
    subimg.height = m_spec.height;
    subimg.bpp = m_bpp;
    if (!m_want_png)
        subimg.len = sizeof (ico_bitmapinfo) // for PNG images this is zero
            + (m_xor_slb + m_and_slb) * m_spec.height;
    subimg.ofs = m_offset;
    if (bigendian()) {
        swap_endian (&subimg.width);
        swap_endian (&subimg.height);
        swap_endian (&subimg.planes);
        swap_endian (&subimg.bpp);
        swap_endian (&subimg.len);
        swap_endian (&subimg.ofs);
    }
    fwrite (&subimg, 1, sizeof(subimg), m_file);

    fseek (m_file, m_offset, SEEK_SET);
    if (m_want_png) {
        // code mostly copied from pngoutput.cpp

        m_png = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (! m_png) {
            close ();
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

        png_set_IHDR (m_png, m_info, m_spec.width, m_spec.height,
                      m_spec.format.size()*8, m_colour_type, PNG_INTERLACE_NONE,
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

        ImageIOParameter *unit=NULL, *xres=NULL, *yres=NULL;
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
    } else {
        // write DIB header
        ico_bitmapinfo bmi;
        memset (&bmi, 0, sizeof (bmi));
        bmi.size = sizeof (bmi);
        bmi.width = m_spec.width;
        // this value is sum of heights of both XOR and AND masks
        bmi.height = m_spec.height * 2;
        bmi.bpp = m_bpp;
        bmi.planes = 1;
        bmi.len = subimg.len - sizeof (ico_bitmapinfo);
        if (bigendian()) {
            // ICOs are little endian
            swap_endian (&bmi.size);
            swap_endian (&bmi.bpp);
            swap_endian (&bmi.width);
            swap_endian (&bmi.height);
            swap_endian (&bmi.len);
        }
        fwrite (&bmi, sizeof (bmi), 1, m_file);

        // append null data so that we don't seek beyond eof in write_scanline
        char buf[512];
        memset (buf, 0, sizeof (buf));
        for (int left = bmi.len; left > 0; left -= sizeof (buf))
            fwrite (buf, left > sizeof (buf) ? sizeof (buf) : left, 1, m_file);
        fseek (m_file, m_offset + sizeof (bmi), SEEK_SET);
    }

    return true;
}



bool
ICOOutput::supports (const std::string &feature) const
{
    // advertise our support for subimages
    if (!strcasecmp (feature.c_str (), "multiimage"))
        return true;
    return false;
}



bool
ICOOutput::put_parameter (const std::string &_name, TypeDesc type,
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
ICOOutput::finish_png_image ()
{
    // Must call this setjmp in every function that does PNG writes
    if (setjmp (png_jmpbuf(m_png))) {
        error ("PNG library error");
        return;
    }
    png_write_end (m_png, NULL);
}



bool
ICOOutput::close ()
{
    if (m_png && m_info) {
        finish_png_image ();
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
                  // Epicly. -- IneQuation
}



bool
ICOOutput::write_scanline (int y, int z, TypeDesc format,
                            const void *data, stride_t xstride)
{
    m_spec.auto_stride (xstride, format, spec().nchannels);
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch);
    if (data == origdata) {
        m_scratch.assign ((unsigned char *)data,
                          (unsigned char *)data + m_spec.scanline_bytes());
        data = &m_scratch[0];
    }

    if (m_want_png) {
        // Must call this setjmp in every function that does PNG writes
        if (setjmp (png_jmpbuf (m_png))) {
            error ("PNG library error");
            return false;
        }
        png_write_row (m_png, (png_byte *)data);
    } else {
        unsigned char buf[4];
        // these are used to read the most significant 8 bits only (the
        // precision loss...), also accounting for byte order
        int mult = format == TypeDesc::UINT16 ? 2 : 1;
        int ofs = (mult > 1 && bigendian()) ? 1 : 0;

        fseek (m_file, m_offset + sizeof (ico_bitmapinfo)
            + (m_spec.height - y - 1) * m_xor_slb, SEEK_SET);
        // write the XOR mask
        for (int x = 0; x < m_spec.width; x++) {
            switch (m_colour_type) {
             // reuse PNG constants
            case PNG_COLOR_TYPE_GRAY:
                buf[0] = buf[1] = buf[2] =
                    ((unsigned char *)data)[x * mult + ofs];
                fwrite (buf, 3, 1, m_file);
                break;
            case PNG_COLOR_TYPE_GRAY_ALPHA:
                buf[0] = buf[1] = buf[2] =
                    ((unsigned char *)data)[mult * (x * 2 + 0) + ofs];
                buf[3] = ((unsigned char *)data)[mult * (x * 2 + 1) + ofs];
                fwrite (buf, 4, 1, m_file);
                break;
            case PNG_COLOR_TYPE_RGB:
                buf[0] = ((unsigned char *)data)[mult * (x * 3 + 2) + ofs];
                buf[1] = ((unsigned char *)data)[mult * (x * 3 + 1) + ofs];
                buf[2] = ((unsigned char *)data)[mult * (x * 3 + 0) + ofs];
                fwrite (buf, 3, 1, m_file);
                break;
            case PNG_COLOR_TYPE_RGB_ALPHA:
                buf[0] = ((unsigned char *)data)[mult * (x * 4 + 2) + ofs];
                buf[1] = ((unsigned char *)data)[mult * (x * 4 + 1) + ofs];
                buf[2] = ((unsigned char *)data)[mult * (x * 4 + 0) + ofs];
                buf[3] = ((unsigned char *)data)[mult * (x * 4 + 3) + ofs];
                fwrite (buf, 4, 1, m_file);
                break;
            }
        }

        fseek (m_file, m_offset + sizeof (ico_bitmapinfo)
            + m_spec.height * m_xor_slb
            + (m_spec.height - y - 1) * m_and_slb, SEEK_SET);
        // write the AND mask
        // only need to do this for images with alpha - 0 is opaque, and we've
        // already filled the file with zeros
        if (m_colour_type != PNG_COLOR_TYPE_GRAY
            && m_colour_type != PNG_COLOR_TYPE_RGB) {
            for (int x = 0; x < m_spec.width; x += 8) {
                buf[0] = 0;
                for (int b = 0; b < 8 && x + b < m_spec.width; b++) {
                    switch (m_colour_type) {
                    case PNG_COLOR_TYPE_GRAY_ALPHA:
                        buf[0] |= ((unsigned char *)data)
                                        [mult * ((x + b) * 2 + 1) + ofs]
                                  <= 127 ? (1 << (7 - b)) : 0;
                        break;
                    case PNG_COLOR_TYPE_RGB_ALPHA:
                        buf[0] |= ((unsigned char *)data)
                                        [mult * ((x + b) * 4 + 3) + ofs]
                                  <= 127 ? (1 << (7 - b)) : 0;
                        break;
                    }
                }
                fwrite (&buf[0], 1, 1, m_file);
            }
        }
    }

    return true;
}
