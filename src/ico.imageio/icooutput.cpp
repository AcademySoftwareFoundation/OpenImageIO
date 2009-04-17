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
    enum {
        COL_GRAY,
        COL_GRAY_ALPHA,
        COL_RGB,
        COL_RGB_ALPHA
    } m_colour_type;                  ///< Requested colour type
    int m_subimage;                   ///< What subimage are we looking at?
    
    // Initialize private members to pre-opened state
    void init (void) {
        m_file = NULL;
        m_subimage = 0;
    }
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
    if (m_spec.format != TypeDesc::UINT8) {
        error ("ICO only supports uint8 pixel data");
        return false;
    }

    switch (m_spec.nchannels) {
    case 1 : m_colour_type = COL_GRAY; break;
    case 2 : m_colour_type = COL_GRAY_ALPHA; break;
    case 3 : m_colour_type = COL_RGB; break;
    case 4 : m_colour_type = COL_RGB_ALPHA; break;
    default:
        error ("ICO only supports 1-4 channels, not %d", m_spec.nchannels);
        return false;
    }

    m_file = fopen (name.c_str(), append ? "a+b" : "wb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    ico_header ico;
    int offset; // offset to the subimage data
    if (!append) {
        // creating new file, write ICO header
        memset (&ico, 0, sizeof(ico));
        ico.type = 1;
        ico.count = 1;
        fwrite (&ico, 1, sizeof(ico), m_file);
        offset = sizeof(ico_header) + sizeof(ico_subimage);
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
        m_subimage = ico.count++;
        fseek (m_file, 0, SEEK_END);
        int len = ftell (m_file);
        unsigned char buf[512];
        // append null data at the end of file so that we don't seek beyond eof
        fwrite (buf, sizeof (ico_subimage), 1, m_file);
        // do the actual moving, 0.5kB per iteration
        int amount;
        for (int left = len - sizeof (ico_header) - sizeof (ico_subimage)
             * (m_subimage - 1); left > 0; left -= sizeof (buf)) {
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
        for (int i = 0; i < m_subimage - 1; i++) {
            fread (&temp, sizeof (temp), 1, m_file);
            if (bigendian())
                swap_endian (&temp);
            temp += sizeof (ico_subimage);
            if (bigendian())
                swap_endian (&temp);
            fseek (m_file, -4, SEEK_CUR);
            fwrite (&temp, sizeof (temp), 1, m_file);
            fseek (m_file, sizeof (ico_subimage) - 4, SEEK_CUR);
        }
        // offset at which we'll be writing new image data
        offset = len + sizeof (ico_subimage);
    }

    // write subimage header
    ico_subimage subimg;
    memset (&subimg, 0, sizeof(subimg));
    subimg.width = m_spec.width;
    subimg.height = m_spec.height;
    subimg.planes = 1;
    subimg.bpp = (m_colour_type == COL_GRAY_ALPHA
                 || m_colour_type == COL_RGB_ALPHA) ? 32 : 24;
    subimg.len = sizeof(ico_bitmapinfo) + subimg.width * subimg.height * (subimg.bpp / 8) + subimg.width * subimg.height / 8;
    subimg.ofs = offset;
    if (bigendian()) {
        swap_endian (&subimg.width);
        swap_endian (&subimg.height);
        swap_endian (&subimg.planes);
        swap_endian (&subimg.bpp);
        swap_endian (&subimg.len);
        swap_endian (&subimg.ofs);
    }
    fwrite (&subimg, 1, sizeof(subimg), m_file);

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
ICOOutput::close ()
{
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
    /*m_spec.auto_stride (xstride, format, spec().nchannels);
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch);
    if (data == origdata) {
        m_scratch.assign ((unsigned char *)data,
                          (unsigned char *)data+m_spec.scanline_bytes());
        data = &m_scratch[0];
    }
    png_write_row (m_png, (png_byte *)data);

    return true;*/
    return false; // FIXME
}
