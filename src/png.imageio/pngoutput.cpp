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

#include "png_pvt.h"

#include "dassert.h"
#include "imageio.h"
#include "strutil.h"

OIIO_PLUGIN_NAMESPACE_BEGIN


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
                       OpenMode mode=Create);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    png_structp m_png;                ///< PNG read structure pointer
    png_infop m_info;                 ///< PNG image info structure pointer
    int m_color_type;                 ///< PNG color model type
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
OIIO_PLUGIN_EXPORTS_BEGIN

DLLEXPORT ImageOutput *png_output_imageio_create () { return new PNGOutput; }

// DLLEXPORT int png_imageio_version = OIIO_PLUGIN_VERSION;   // it's in pnginput.cpp

DLLEXPORT const char * png_output_extensions[] = {
    "png", NULL
};

OIIO_PLUGIN_EXPORTS_END



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
PNGOutput::open (const std::string &name, const ImageSpec &userspec,
                 OpenMode mode)
{
    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    close ();  // Close any already-opened file
    m_spec = userspec;  // Stash the spec

    m_file = fopen (name.c_str(), "wb");
    if (! m_file) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }

    std::string s = PNG_pvt::create_write_struct (m_png, m_info,
                                                  m_color_type, m_spec);
    if (s.length ()) {
        close ();
        error ("%s", s.c_str ());
        return false;
    }

    png_init_io (m_png, m_file);
    png_set_compression_level (m_png, 6 /* medium speed vs size tradeoff */);

    PNG_pvt::write_info (m_png, m_info, m_color_type, m_spec, m_pngtext);

    return true;
}



bool
PNGOutput::close ()
{
    if (m_png)
        PNG_pvt::finish_image (m_png);
    PNG_pvt::destroy_write_struct (m_png, m_info);

    if (m_file) {
        fclose (m_file);
        m_file = NULL;
    }

    init ();      // re-initialize
    return true;  // How can we fail?
}



template <class T>
static void 
deassociateAlpha (T * data, int size, int channels, int alpha_channel, float gamma)
{
    unsigned int max = std::numeric_limits<T>::max();
    if (gamma == 1){
        for (int x = 0;  x < size;  ++x, data += channels)
            if (data[alpha_channel])
                for (int c = 0;  c < channels;  c++)
                    if (c != alpha_channel) {
                        unsigned int f = data[c];
                        f = (f * max) / data[alpha_channel];
                        data[c] = (T) std::min (max, f);
                    }
    }
    else {
        for (int x = 0;  x < size;  ++x, data += channels)
            if (data[alpha_channel]) {
                // See associateAlpha() for an explanation.
                float alpha_deassociate = pow((float)max / data[alpha_channel],
                                              gamma);
                for (int c = 0;  c < channels;  c++)
                    if (c != alpha_channel)
                        data[c] = static_cast<T> (std::min (max,
                                (unsigned int)(data[c] * alpha_deassociate)));
            }
    }
}



bool
PNGOutput::write_scanline (int y, int z, TypeDesc format,
                            const void *data, stride_t xstride)
{
    y -= m_spec.y;
    m_spec.auto_stride (xstride, format, spec().nchannels);
    const void *origdata = data;
    data = to_native_scanline (format, data, xstride, m_scratch);
    if (data == origdata) {
        m_scratch.assign ((unsigned char *)data,
                          (unsigned char *)data+m_spec.scanline_bytes());
        data = &m_scratch[0];
    }

    // PNG specifically dictates unassociated (un-"premultiplied") alpha
    if (m_spec.alpha_channel != -1) {
        float gamma = m_spec.get_float_attribute ("oiio:Gamma", 1.0f);
        if (m_spec.format == TypeDesc::UINT16)
            deassociateAlpha ((unsigned short *)data, m_spec.width,
                              m_spec.nchannels, m_spec.alpha_channel,
                              gamma);
        else
            deassociateAlpha ((unsigned char *)data, m_spec.width,
                              m_spec.nchannels, m_spec.alpha_channel,
                              gamma);
    }

    // PNG is always big endian
    if (littleendian() && m_spec.format == TypeDesc::UINT16)
        swap_endian ((unsigned short *)data, m_spec.width*m_spec.nchannels);

    if (!PNG_pvt::write_row (m_png, (png_byte *)data)) {
        error ("PNG library error");
        return false;
    }

    return true;
}

OIIO_PLUGIN_NAMESPACE_END

