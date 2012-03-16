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
#include <zlib.h>

#include "ico.h"
#include "../png.imageio/png_pvt.h"

#include "dassert.h"
#include "typedesc.h"
#include "imageio.h"
#include "thread.h"
#include "strutil.h"
#include "fmath.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

using namespace ICO_pvt;

class ICOOutput : public ImageOutput {
public:
    ICOOutput ();
    virtual ~ICOOutput ();
    virtual const char * format_name (void) const { return "ico"; }
    virtual bool supports (const std::string &feature) const;
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    int m_color_type;                 ///< Requested colour type
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

    /// Helper: read, with error detection
    ///
    bool fread (void *buf, size_t itemsize, size_t nitems) {
        size_t n = ::fread (buf, itemsize, nitems, m_file);
        if (n != nitems)
            error ("Read error");
        return n == nitems;
    }
};




// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageOutput *ico_output_imageio_create () { return new ICOOutput; }

// DLLEXPORT int ico_imageio_version = OIIO_PLUGIN_VERSION;   // it's in icoinput.cpp

DLLEXPORT const char * ico_output_extensions[] = {
    "ico", NULL
};

OIIO_PLUGIN_EXPORTS_END



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
ICOOutput::open (const std::string &name, const ImageSpec &userspec,
                 OpenMode mode)
{
    if (mode == AppendMIPLevel) {
        error ("%s does not support MIP levels", format_name());
        return false;
    }

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

    // check if the client wants this subimage written as PNG
    // also force PNG if image size is 256 because ico_header->width and height
    // are 8-bit
    const ImageIOParameter *p = m_spec.find_attribute ("ico:PNG",
                                                       TypeDesc::TypeInt);
    m_want_png = (p && *(int *)p->data())
                 || m_spec.width == 256 || m_spec.height == 256;

    if (m_want_png) {
        std::string s = PNG_pvt::create_write_struct (m_png, m_info,
                                                      m_color_type, m_spec);
        if (s.length ()) {
            error ("%s", s.c_str ());
            return false;
        }
    } else {
        // reuse PNG constants for DIBs as well
        switch (m_spec.nchannels) {
        case 1 : m_color_type = PNG_COLOR_TYPE_GRAY; break;
        case 2 : m_color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
        case 3 : m_color_type = PNG_COLOR_TYPE_RGB; break;
        case 4 : m_color_type = PNG_COLOR_TYPE_RGB_ALPHA; break;
        default:
            error ("ICO only supports 1-4 channels, not %d", m_spec.nchannels);
            return false;
        }

        m_bpp = (m_color_type == PNG_COLOR_TYPE_GRAY_ALPHA
                || m_color_type == PNG_COLOR_TYPE_RGB_ALPHA) ? 32 : 24;
        m_xor_slb = (m_spec.width * m_bpp + 7) / 8 // real data bytes
                    + (4 - ((m_spec.width * m_bpp + 7) / 8) % 4) % 4; // padding
        m_and_slb = (m_spec.width + 7) / 8 // real data bytes
                    + (4 - ((m_spec.width + 7) / 8) % 4) % 4; // padding

        // Force 8 bit integers
        if (m_spec.format != TypeDesc::UINT8)
            m_spec.set_format (TypeDesc::UINT8);
    }

    //std::cerr << "[ico] writing at " << m_bpp << "bpp\n";

    m_file = fopen (name.c_str(), mode == AppendSubimage ? "r+b" : "wb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    ico_header ico;
    if (mode == Create) {
        // creating new file, write ICO header
        memset (&ico, 0, sizeof(ico));
        ico.type = 1;
        ico.count = 1;
        if (bigendian()) {
            // ICOs are little endian
            swap_endian (&ico.type);
            swap_endian (&ico.count);
        }
        fwrite (&ico, 1, sizeof(ico), m_file);
        m_offset = sizeof(ico_header) + sizeof(ico_subimage);
    } else {
        // we'll be appending data, so see what's already in the file
        if (! fread (&ico, 1, sizeof(ico)))
            return false;
        if (bigendian()) {
            // ICOs are little endian
            swap_endian (&ico.type);
            swap_endian (&ico.count);
        }

        /*std::cerr << "[ico] reserved = " << ico.reserved << " type = "
                  << ico.type << " count = " << ico.count << "\n";*/

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
        int amount, skip = sizeof (ico_header) + sizeof (ico_subimage)
                           * (subimage - 1);
        for (int left = len - skip; left > 0; left -= sizeof (buf)) {
            amount = std::min (left, (int)sizeof (buf));
            /*std::cerr << "[ico] moving " << amount << " bytes (" << left
                      << " vs " << sizeof (buf) << ")\n";*/
            fseek (m_file, skip + left - amount, SEEK_SET);
            if (! fread (buf, amount, 1))
                return false;
            fseek (m_file, skip + left - amount + sizeof (ico_subimage),
                   SEEK_SET);
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
            if (! fread (&temp, sizeof (temp), 1))
                return false;
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
        png_init_io (m_png, m_file);
        png_set_compression_level (m_png, Z_BEST_COMPRESSION);

        PNG_pvt::write_info (m_png, m_info, m_color_type, m_spec, m_pngtext);
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
            fwrite (buf, std::min (left, (int)sizeof (buf)), 1, m_file);
        fseek (m_file, m_offset + sizeof (bmi), SEEK_SET);
    }

    return true;
}



bool
ICOOutput::supports (const std::string &feature) const
{
    // advertise our support for subimages
    if (Strutil::iequals (feature, "multiimage"))
        return true;
    return false;
}



bool
ICOOutput::close ()
{
    //std::cerr << "[ico] closing\n";
    if (m_png && m_info) {
        PNG_pvt::finish_image (m_png);
        PNG_pvt::destroy_write_struct (m_png, m_info);
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
        if (!PNG_pvt::write_row (m_png, (png_byte *)data)) {
            error ("PNG library error");
            return false;
        }
    } else {
        unsigned char *bdata = (unsigned char *)data;
        unsigned char buf[4];

        fseek (m_file, m_offset + sizeof (ico_bitmapinfo)
            + (m_spec.height - y - 1) * m_xor_slb, SEEK_SET);
        // write the XOR mask
        for (int x = 0; x < m_spec.width; x++) {
            switch (m_color_type) {
             // reuse PNG constants
            case PNG_COLOR_TYPE_GRAY:
                buf[0] = buf[1] = buf[2] = bdata[x];
                fwrite (buf, 3, 1, m_file);
                break;
            case PNG_COLOR_TYPE_GRAY_ALPHA:
                buf[0] = buf[1] = buf[2] = bdata[x * 2 + 0];
                buf[3] = bdata[x * 2 + 1];
                fwrite (buf, 4, 1, m_file);
                break;
            case PNG_COLOR_TYPE_RGB:
                buf[0] = bdata[x * 3 + 2];
                buf[1] = bdata[x * 3 + 1];
                buf[2] = bdata[x * 3 + 0];
                fwrite (buf, 3, 1, m_file);
                break;
            case PNG_COLOR_TYPE_RGB_ALPHA:
                buf[0] = bdata[x * 4 + 2];
                buf[1] = bdata[x * 4 + 1];
                buf[2] = bdata[x * 4 + 0];
                buf[3] = bdata[x * 4 + 3];
                fwrite (buf, 4, 1, m_file);
                break;
            }
        }

        fseek (m_file, m_offset + sizeof (ico_bitmapinfo)
            + m_spec.height * m_xor_slb
            + (m_spec.height - y - 1) * m_and_slb, SEEK_SET);
        // write the AND mask
        // It's required even for 32-bit images because it can be used when
        // drawing at colour depths lower than 24-bit. If it's not present,
        // Windows will read out-of-bounds, treating any data that it
        // encounters as the AND mask, resulting in ugly transparency effects.
        // Only need to do this for images with alpha, becasue 0 is opaque,
        // and we've already filled the file with zeros.
        if (m_color_type != PNG_COLOR_TYPE_GRAY
            && m_color_type != PNG_COLOR_TYPE_RGB) {
            for (int x = 0; x < m_spec.width; x += 8) {
                buf[0] = 0;
                for (int b = 0; b < 8 && x + b < m_spec.width; b++) {
                    switch (m_color_type) {
                    case PNG_COLOR_TYPE_GRAY_ALPHA:
                        buf[0] |= bdata[(x + b) * 2 + 1]
                                  <= 127 ? (1 << (7 - b)) : 0;
                        break;
                    case PNG_COLOR_TYPE_RGB_ALPHA:
                        buf[0] |= bdata[(x + b) * 4 + 3]
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

OIIO_PLUGIN_NAMESPACE_END

