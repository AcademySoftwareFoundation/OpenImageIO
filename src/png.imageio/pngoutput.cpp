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
#include <ctime>

#include "png_pvt.h"


OIIO_PLUGIN_NAMESPACE_BEGIN


class PNGOutput final : public ImageOutput {
public:
    PNGOutput ();
    virtual ~PNGOutput ();
    virtual const char * format_name (void) const { return "png"; }
    virtual int supports (string_view feature) const {
        return (feature == "alpha");
    }
    virtual bool open (const std::string &name, const ImageSpec &spec,
                       OpenMode mode=Create);
    virtual bool close ();
    virtual bool write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride);
    virtual bool write_tile (int x, int y, int z, TypeDesc format,
                             const void *data, stride_t xstride,
                             stride_t ystride, stride_t zstride);

private:
    std::string m_filename;           ///< Stash the filename
    FILE *m_file;                     ///< Open image handle
    png_structp m_png;                ///< PNG read structure pointer
    png_infop m_info;                 ///< PNG image info structure pointer
    unsigned int m_dither;
    int m_color_type;                 ///< PNG color model type
    bool m_convert_alpha;             ///< Do we deassociate alpha?
    float m_gamma;                    ///< Gamma to use for alpha conversion
    std::vector<unsigned char> m_scratch;
    std::vector<png_text> m_pngtext;
    std::vector<unsigned char> m_tilebuffer;

    // Initialize private members to pre-opened state
    void init (void) {
        m_file = NULL;
        m_png = NULL;
        m_info = NULL;
        m_convert_alpha = true;
        m_gamma = 1.0;
        m_pngtext.clear ();
    }

    // Add a parameter to the output
    bool put_parameter (const std::string &name, TypeDesc type,
                        const void *data);

    void finish_image ();
};




// Obligatory material to make this a recognizeable imageio plugin:
OIIO_PLUGIN_EXPORTS_BEGIN

OIIO_EXPORT ImageOutput *png_output_imageio_create () { return new PNGOutput; }

// OIIO_EXPORT int png_imageio_version = OIIO_PLUGIN_VERSION;   // it's in pnginput.cpp

OIIO_EXPORT const char * png_output_extensions[] = {
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

    // If not uint8 or uint16, default to uint8
    if (m_spec.format != TypeDesc::UINT8 && m_spec.format != TypeDesc::UINT16)
        m_spec.set_format (TypeDesc::UINT8);

    m_file = Filesystem::fopen (name, "wb");
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
    png_set_compression_level (m_png, std::max (std::min (m_spec.get_int_attribute ("png:compressionLevel", 6/* medium speed vs size tradeoff */), Z_BEST_COMPRESSION), Z_NO_COMPRESSION));
    std::string compression = m_spec.get_string_attribute ("compression");
    if (compression.empty ()) {
        png_set_compression_strategy(m_png, Z_DEFAULT_STRATEGY);
    }
    else if (Strutil::iequals (compression, "default")) {
        png_set_compression_strategy(m_png, Z_DEFAULT_STRATEGY);
    }
    else if (Strutil::iequals (compression, "filtered")) {
        png_set_compression_strategy(m_png, Z_FILTERED);
    }
    else if (Strutil::iequals (compression, "huffman")) {
        png_set_compression_strategy(m_png, Z_HUFFMAN_ONLY);
    }
    else if (Strutil::iequals (compression, "rle")) {
        png_set_compression_strategy(m_png, Z_RLE);
    }
    else if (Strutil::iequals (compression, "fixed")) {
        png_set_compression_strategy(m_png, Z_FIXED);
    }
    else {
        png_set_compression_strategy(m_png, Z_DEFAULT_STRATEGY);
    }

    PNG_pvt::write_info (m_png, m_info, m_color_type, m_spec, m_pngtext,
                         m_convert_alpha, m_gamma);

    m_dither = (m_spec.format == TypeDesc::UINT8) ?
                    m_spec.get_int_attribute ("oiio:dither", 0) : 0;

    m_convert_alpha = m_spec.alpha_channel != -1 &&
                      !m_spec.get_int_attribute("oiio:UnassociatedAlpha", 0);

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize (m_spec.image_bytes());

    return true;
}



bool
PNGOutput::close ()
{
    if (! m_file) {   // already closed
        init ();
        return true;
    }

    bool ok = true;
    if (m_spec.tile_width) {
        // Handle tile emulation -- output the buffered pixels
        ASSERT (m_tilebuffer.size());
        ok &= write_scanlines (m_spec.y, m_spec.y+m_spec.height, 0,
                               m_spec.format, &m_tilebuffer[0]);
        std::vector<unsigned char>().swap (m_tilebuffer);
    }

    if (m_png)
        PNG_pvt::finish_image (m_png);
    PNG_pvt::destroy_write_struct (m_png, m_info);

    fclose (m_file);
    m_file = NULL;

    init ();      // re-initialize
    return ok;
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
    data = to_native_scanline (format, data, xstride, m_scratch,
                               m_dither, y, z);
    if (data == origdata) {
        m_scratch.assign ((unsigned char *)data,
                          (unsigned char *)data+m_spec.scanline_bytes());
        data = &m_scratch[0];
    }

    // PNG specifically dictates unassociated (un-"premultiplied") alpha
    if (m_convert_alpha) {
        if (m_spec.format == TypeDesc::UINT16)
            deassociateAlpha ((unsigned short *)data, m_spec.width,
                              m_spec.nchannels, m_spec.alpha_channel,
                              m_gamma);
        else
            deassociateAlpha ((unsigned char *)data, m_spec.width,
                              m_spec.nchannels, m_spec.alpha_channel,
                              m_gamma);
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



bool
PNGOutput::write_tile (int x, int y, int z, TypeDesc format,
                       const void *data, stride_t xstride,
                       stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer (x, y, z, format, data, xstride,
                                      ystride, zstride, &m_tilebuffer[0]);
}


OIIO_PLUGIN_NAMESPACE_END

