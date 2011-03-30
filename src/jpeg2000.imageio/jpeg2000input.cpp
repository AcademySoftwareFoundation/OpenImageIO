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

#include "imageio.h"
#include "jpeg2000_pvt.h"

OIIO_PLUGIN_NAMESPACE_BEGIN

// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN

    DLLEXPORT int jpeg2000_imageio_version = OIIO_PLUGIN_VERSION;
    DLLEXPORT ImageInput *jpeg2000_input_imageio_create () {
        return new Jpeg2000Input;
    }
    DLLEXPORT const char *jpeg2000_input_extensions[] = {
        "jp2", "j2k", NULL
    };

OIIO_PLUGIN_EXPORTS_END



void
Jpeg2000Input::init (void)
{
    m_scanline_size = 0;
    m_stream = NULL;
    m_image = NULL;
    m_fam_clrspc = JAS_CLRSPC_UNKNOWN;
    m_cmpt_id.clear ();
    m_matrix_chan.clear ();
    m_pixels.clear ();
    jas_init ();
}



bool
Jpeg2000Input::read_channels (void)
{
    m_cmpt_id.resize(m_spec.nchannels, 0);
    m_matrix_chan.resize (m_spec.nchannels, NULL);

    // RGB family color space
    if (m_fam_clrspc == JAS_CLRSPC_FAM_RGB) {
        for (int i = 0; i < m_spec.nchannels; ++i)
            m_matrix_chan[i] = jas_matrix_create (m_spec.height, m_spec.width);
        m_cmpt_id[RED] = jas_image_getcmptbytype (m_image, JAS_IMAGE_CT_RGB_R);
        m_cmpt_id[GREEN] = jas_image_getcmptbytype (m_image, JAS_IMAGE_CT_RGB_G);
        m_cmpt_id[BLUE] = jas_image_getcmptbytype (m_image, JAS_IMAGE_CT_RGB_B);
        // reading red, green and blue component
        jas_image_readcmpt (m_image, m_cmpt_id[RED], 0, 0, m_spec.width,
                            m_spec.height, m_matrix_chan[RED]);
        jas_image_readcmpt (m_image, m_cmpt_id[GREEN], 0, 0, m_spec.width,
                            m_spec.height, m_matrix_chan[GREEN]);
        jas_image_readcmpt (m_image, m_cmpt_id[BLUE], 0, 0, m_spec.width,
                            m_spec.height, m_matrix_chan[BLUE]);
    }
    // Greyscale
    else if (m_fam_clrspc == JAS_CLRSPC_FAM_GRAY) {
        m_matrix_chan[GREY] = jas_matrix_create (m_spec.height, m_spec.width);
        m_cmpt_id[GREY] = jas_image_getcmptbytype (m_image, JAS_IMAGE_CT_GRAY_Y);
        // reading greyscale component
        jas_image_readcmpt (m_image, m_cmpt_id[GREY], 0, 0, m_spec.width,
                            m_spec.height, m_matrix_chan[GREY]);
    }
    return true;
}



bool
Jpeg2000Input::open (const std::string &name, ImageSpec &spec)
{
    // saving 'name' and 'spec' for later use
    m_filename = name;

    // check if file exist and can be opened as JasPer stream
    m_stream = jas_stream_fopen ( name.c_str(), "rb");
    if (! m_stream) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }
    // checking if the file is JPEG2000 file
    int fmt = jas_image_getfmt (m_stream);
    const char *format = jas_image_fmttostr (fmt);
    if (! format || (strcmp (format, JP2_STREAM) && strcmp (format, JPC_STREAM))) {
        error ("%s is not a %s file", name.c_str (), format_name());
        close ();
        return false;
    }

    // decompressing the image
    m_image = jas_image_decode (m_stream, fmt, NULL);
    if (! m_image) {
        error ("Could not decode image");
        close ();
        return false;
    }
    // getting basic information about image
    int width = jas_image_width (m_image);
    int height = jas_image_height (m_image);
    int channels = jas_image_numcmpts (m_image);
    if (channels > 4) {
        error ("plugin currently desn't support images with more than 4 channels");
        close ();
        return false;
    }
    m_spec = ImageSpec (width, height, channels, TypeDesc::UINT8);
    m_spec.attribute("jpeg2000:streamformat", format);

    // what family of color space was used
    m_fam_clrspc = jas_clrspc_fam (jas_image_clrspc(m_image));

    read_channels ();

    // getting number of bits per channel
    m_prec = jas_image_cmptprec(m_image, m_cmpt_id[RED]);

    if(m_prec == 12 || m_prec == 16)
        m_spec.set_format(TypeDesc::UINT16);

    // stuff used in read_native_scanline
    m_scanline_size = m_spec.scanline_bytes();
    m_pixels.resize (m_scanline_size, 0);

    spec = m_spec;
    return true;
}



bool
Jpeg2000Input::read_native_scanline (int y, int z, void *data)
{
    uint r,g,b;

    memset (&m_pixels[0], 0, m_pixels.size ());
    if (m_fam_clrspc == JAS_CLRSPC_FAM_GRAY) {
        for (int i = 0; i < m_spec.width; ++i)
            m_pixels[i] = jas_matrix_get (m_matrix_chan[GREY], y, i);
    }
    else if (m_fam_clrspc == JAS_CLRSPC_FAM_RGB) {
        for (int i = 0, pos = 0; i < m_spec.width; i++) {

            r = jas_matrix_get (m_matrix_chan[RED], y, i);
            g = jas_matrix_get (m_matrix_chan[GREEN], y, i);
            b = jas_matrix_get (m_matrix_chan[BLUE], y, i);


            if(m_prec == 8){
               m_pixels[pos++] = r;
               m_pixels[pos++] = g;
               m_pixels[pos++] = b;

               if (m_spec.nchannels == 4)
                    pos++;
            }
            else if(m_prec == 12){
                m_pixels[pos++] = r & 0xF;
                m_pixels[pos++] = r>>4;
                m_pixels[pos++] = g & 0xF;
                m_pixels[pos++] = g>>4;
                m_pixels[pos++] = b & 0xF;
                m_pixels[pos++] = b>>4;

                if (m_spec.nchannels == 4)
                    pos+=2;
            }
            else if(m_prec == 16){
                m_pixels[pos++] = r & 0xFF;
                m_pixels[pos++] = r>>8;
                m_pixels[pos++] = g & 0xFF;
                m_pixels[pos++] = g>>8;
                m_pixels[pos++] = b & 0xFF;
                m_pixels[pos++] = b>>8;

                printf("good %d", r & 0xFF);

                if (m_spec.nchannels == 4)
                    pos+=2;
            }

        }
    }
    memcpy(data, &m_pixels[0], m_scanline_size);
    return true;
}



inline bool
Jpeg2000Input::close (void)
{
    if (m_stream)
        jas_stream_close (m_stream);
    if (m_image)
        jas_image_destroy (m_image);
    for (size_t i = 0; i < m_matrix_chan.size (); ++i) {
        if (m_matrix_chan[i])
            jas_matrix_destroy (m_matrix_chan[i]);
    }
    init ();
    jas_cleanup ();
    return true;
}

OIIO_PLUGIN_NAMESPACE_END

