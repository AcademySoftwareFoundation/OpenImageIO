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

    DLLEXPORT ImageOutput *jpeg2000_output_imageio_create () {
        return new Jpeg2000Output;
    }
    DLLEXPORT const char *jpeg2000_output_extensions[] = {
        "jp2", "j2k", NULL
    };

OIIO_PLUGIN_EXPORTS_END



void
Jpeg2000Output::init(void)
{
    jas_init ();
    m_image = NULL;
    m_components = NULL;
    m_stream = NULL;
    m_scanline_size = 0;
    m_scanline.clear ();
    m_pixels.clear ();
}



void
Jpeg2000Output::component_struct_init (jas_image_cmptparm_t *cmpt) {
    cmpt->tlx = 0;
    cmpt->tly = 0;
    cmpt->hstep = 1;
    cmpt->vstep = 1;
    cmpt->width = m_spec.width;
    cmpt->height = m_spec.height;
    // add other precisions!
    cmpt->prec = 8;
}



bool
Jpeg2000Output::open (const std::string &name, const ImageSpec &spec,
                      OpenMode mode)
{
    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    // saving 'name' and 'spec' for later use
    m_spec = spec;
    m_filename = name;

    // check for things that this format doesn't support
    if (m_spec.width < 1 || m_spec.height < 1) {
        error ("Image resolution must be at least 1x1, you asked for %d x %d",
               m_spec.width, m_spec.height);
        return false;
    }
    if (m_spec.depth > 1) {
        error ("%s doesn't support volume images (depth > 1)", format_name ());
        return false;
    }
    else
        m_spec.depth = 1;
    if (m_spec.nchannels > 4) {
        error ("Plugin currently doesn't support images with more then 4 components");
        return false;
    }

    // opening JasPer stream for writing jpeg2000 data to file
    m_stream = jas_stream_fopen (m_filename.c_str(), "wb");
    if (! m_stream) {
        error ("Could not open file \"%s\"", name.c_str());
        return false;
    }
    // here we create structures that holds informations about
    // each channel of the image separately and then fill them
    // with defaults values
    m_components = new jas_image_cmptparm_t[m_spec.nchannels];
    for (int i = 0; i < m_spec.nchannels; ++i)
        component_struct_init (&m_components[i]);
    // creating output image that will hold informations about all
    // components of the image. This data will be saved to file
    // through JasPer stream
    m_image = jas_image_create (m_spec.nchannels, m_components, JAS_CLRSPC_UNKNOWN);
    if (! m_image) {
        error("Could not create output image due to error in memory allocation.");
        close ();
        return false;
    }
    // forcing Gray colorspace when we have one channel
    if (m_spec.nchannels == 1) {
        jas_image_setclrspc (m_image, JAS_CLRSPC_SGRAY);
        jas_image_setcmpttype (m_image, 0, JAS_IMAGE_CT_GRAY_Y);
    }
    // forcing RGB when we have three channels
    // and eventually adding opacity channel
    else if (m_spec.nchannels >= 3) {
        jas_image_setclrspc (m_image, JAS_CLRSPC_SRGB);
        jas_image_setcmpttype (m_image, 0, JAS_IMAGE_CT_RGB_R);
        jas_image_setcmpttype (m_image, 1, JAS_IMAGE_CT_RGB_G);
        jas_image_setcmpttype (m_image, 2, JAS_IMAGE_CT_RGB_B);
        if (m_spec.nchannels == 4)
            jas_image_setcmpttype (m_image, 3, JAS_IMAGE_CT_OPACITY);
    }

    // forcing UINT8 format
    m_spec.set_format (TypeDesc::UINT8);

    stream_format = m_spec.get_string_attribute("Stream format", "none");
    if (!strcmp (stream_format.c_str(), "none")) {
        stream_format.erase (stream_format.begin(), stream_format.end());
        stream_format.assign("jpc", 3);
        m_spec.attribute ("Stream format", stream_format.c_str());
    }

    // stuff used in write_scanline() method
    // here we store informations about pixels from one row
    // this data will be compressed later
    m_scanline.resize (m_spec.nchannels, 0);
    for (int i = 0; i < m_spec.nchannels; ++i)
        m_scanline[i] = jas_matrix_create (1, m_spec.width);
    m_scanline_size = m_spec.scanline_bytes();
    m_pixels.resize (m_scanline_size, 0);
    return true;
}



bool
Jpeg2000Output::write_scanline (int y, int z, TypeDesc format,
                                 const void *data, stride_t xstride)
{
    if (y >= m_spec.height) {
        error ("Attempt to write too many scanlines to %s", m_filename.c_str());
        return false;
    }
    data = to_native_scanline (format, data, xstride, m_scratch);
    memset (&m_pixels[0], 0, m_pixels.size());
    memcpy (&m_pixels[0], data, m_scanline_size);
    if (m_spec.nchannels == 1) {
        // if the memory for scanline wasn't allocated we do nothing
        if (! m_scanline[GREY])
            return false;
        // before we save data to file we have to information about
        // component to jas_matrix_t structure (m_scanline) and then
        // write it to the jas_image_t structure (m_image)
        for (size_t i = 0; i < m_scanline_size; ++i)
            jas_matrix_set (m_scanline[GREY], 0, i, m_pixels[i]);
        jas_image_writecmpt (m_image, 0, 0, y, m_spec.width, 1, m_scanline[GREY]);
    }
    else if (m_spec.nchannels >= 3) {
        if (!m_scanline[RED] || !m_scanline[GREEN] || !m_scanline[BLUE])
            return false;
        for (int i = 0, pos=0; i < (int)m_scanline_size; i+=m_spec.nchannels) {
            jas_matrix_set (m_scanline[RED], 0, pos, m_pixels[i]);
            jas_matrix_set (m_scanline[GREEN], 0, pos, m_pixels[i+1]);
            jas_matrix_set (m_scanline[BLUE], 0, pos, m_pixels[i+2]);
            if (m_spec.nchannels == 4)
                jas_matrix_set (m_scanline[OPACITY], 0, pos, m_pixels[i+3]);
            pos++;
        }
        jas_image_writecmpt (m_image, RED, 0, y, m_spec.width, 1,
                             m_scanline[RED]);
        jas_image_writecmpt (m_image, GREEN, 0, y, m_spec.width, 1,
                             m_scanline[GREEN]);
        jas_image_writecmpt (m_image, BLUE, 0, y, m_spec.width, 1,
                             m_scanline[BLUE]);
        if (m_spec.nchannels == 4)
            jas_image_writecmpt (m_image, OPACITY, 0, y, m_spec.width, 1,
                                 m_scanline[OPACITY]);
    }
    // after copying last row we save the data to the file
    // jas_image_encode will save all important informations
    // (like magic_numbers, headers, etc.
    if (y == m_spec.height - 1) {
        // FIXME (robertm): shoud know if we want jpc or jp2
        // set this in Jpeg2000Input and check here?
        if (jas_image_encode (m_image, m_stream, jas_image_strtofmt((char*)stream_format.c_str()), (char*)"") < 0) {
            error("couldn't encode image");
            return false;
        }
    }
    return true;
}



bool
Jpeg2000Output::close ()
{
    if (m_stream)
        jas_stream_close(m_stream);
    if (m_image)
        jas_image_destroy (m_image);
    for (size_t i = 0; i < m_scanline.size (); ++i) {
        if (m_scanline[i])
          jas_matrix_destroy (m_scanline[i]);
    }
    delete [] m_components;
    init ();
    jas_cleanup ();
    return true;
}

OIIO_PLUGIN_NAMESPACE_END

