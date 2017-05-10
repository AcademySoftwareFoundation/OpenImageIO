/*
  Copyright 2010 Larry Gritz and the other authors and contributors.
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

#include "sgi_pvt.h"


OIIO_PLUGIN_NAMESPACE_BEGIN

// Obligatory material to make this a recognizeable imageio plugin
OIIO_PLUGIN_EXPORTS_BEGIN
    OIIO_EXPORT ImageOutput *sgi_output_imageio_create () {
        return new SgiOutput;
    }
    OIIO_EXPORT const char *sgi_output_extensions[] = {
        "sgi", "rgb", "rgba", "bw", "int", "inta", NULL
    };
OIIO_PLUGIN_EXPORTS_END



int
SgiOutput::supports (string_view feature) const
{
    return (feature == "alpha"
         || feature == "nchannels");
}



bool
SgiOutput::open (const std::string &name, const ImageSpec &spec,
                 OpenMode mode)
{
    if (mode != Create) {
        error ("%s does not support subimages or MIP levels", format_name());
        return false;
    }

    close ();  // Close any already-opened file
    // saving 'name' and 'spec' for later use
    m_filename = name;
    m_spec = spec;

    m_fd = Filesystem::fopen (m_filename, "wb");
    if (!m_fd) {
        error ("Unable to open file \"%s\"", m_filename.c_str ());
        return false;
    }

    // SGI image files only supports UINT8 and UINT16.  If something
    // else was requested, revert to the one most likely to be readable
    // by any SGI reader: UINT8
    if (m_spec.format != TypeDesc::UINT8 && m_spec.format != TypeDesc::UINT16)
        m_spec.set_format (TypeDesc::UINT8);
    m_dither = (m_spec.format == TypeDesc::UINT8) ?
                    m_spec.get_int_attribute ("oiio:dither", 0) : 0;

    // If user asked for tiles -- which this format doesn't support, emulate
    // it by buffering the whole image.
    if (m_spec.tile_width && m_spec.tile_height)
        m_tilebuffer.resize (m_spec.image_bytes());

    return create_and_write_header();
}



bool
SgiOutput::write_scanline (int y, int z, TypeDesc format, const void *data,
                           stride_t xstride)
{
    y = m_spec.height - y - 1;
    data = to_native_scanline (format, data, xstride, m_scratch,
                               m_dither, y, z);

    // In SGI format all channels are saved to file separately: firsty all
    // channel 1 scanlines are saved, then all channel2 scanlines are saved
    // and so on.
    //
    // Note that since SGI images are pretty archaic and most probably
    // people won't be too picky about full flexibility writing them, we
    // content ourselves with only writing uncompressed data, and don't
    // attempt to write with RLE encoding.

    int bpc = m_spec.format.size();  // bytes per channel
    std::vector<unsigned char> channeldata (m_spec.width * bpc);

    for (int c = 0;  c < m_spec.nchannels;  ++c) {
        unsigned char *cdata = (unsigned char *)data + c*bpc;
        for (int x = 0;  x < m_spec.width;  ++x) {
            channeldata[x*bpc] = cdata[0];
            if (bpc == 2)
                channeldata[x*bpc+1] = cdata[1];
            cdata += m_spec.nchannels * bpc;  // advance to next pixel
        }
        if (bpc == 2 && littleendian())
            swap_endian ((unsigned short *)&channeldata[0], m_spec.width);
        long scanline_offset = sgi_pvt::SGI_HEADER_LEN + (c * m_spec.height + y)
                                  * m_spec.width * bpc;
        fseek (m_fd, scanline_offset, SEEK_SET);
        if (!fwrite (&channeldata[0], 1, m_spec.width * bpc)) {
            return false;
        }
    }

    return true;    
}



bool
SgiOutput::write_tile (int x, int y, int z, TypeDesc format,
                       const void *data, stride_t xstride,
                       stride_t ystride, stride_t zstride)
{
    // Emulate tiles by buffering the whole image
    return copy_tile_to_image_buffer (x, y, z, format, data, xstride,
                                      ystride, zstride, &m_tilebuffer[0]);
}



bool
SgiOutput::close ()
{
    if (! m_fd) {   // already closed
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

    fclose (m_fd);
    init ();
    return ok;
}



bool
SgiOutput::create_and_write_header()
{
    sgi_pvt::SgiHeader sgi_header;
    sgi_header.magic = sgi_pvt::SGI_MAGIC;
    sgi_header.storage = sgi_pvt::VERBATIM;
    sgi_header.bpc = m_spec.format.size();

    if (m_spec.height == 1 && m_spec.nchannels == 1)
        sgi_header.dimension = sgi_pvt::ONE_SCANLINE_ONE_CHANNEL;
    else if (m_spec.nchannels == 1)
        sgi_header.dimension = sgi_pvt::MULTI_SCANLINE_ONE_CHANNEL;
    else 
        sgi_header.dimension = sgi_pvt::MULTI_SCANLINE_MULTI_CHANNEL;

    sgi_header.xsize = m_spec.width;
    sgi_header.ysize = m_spec.height;
    sgi_header.zsize = m_spec.nchannels;
    sgi_header.pixmin = 0;
    sgi_header.pixmax = (sgi_header.bpc == 1) ? 255 : 65535;
    sgi_header.dummy = 0;

    ParamValue *ip = m_spec.find_attribute ("ImageDescription",
                                                   TypeDesc::STRING);
    if (ip && ip->data()) {
        const char** img_descr = (const char**)ip->data();
        strncpy (sgi_header.imagename, *img_descr, 80);
        sgi_header.imagename[79] = 0;
    }

    sgi_header.colormap = sgi_pvt::NORMAL;

    if (littleendian()) {
        swap_endian(&sgi_header.magic);
        swap_endian(&sgi_header.dimension);
        swap_endian(&sgi_header.xsize);
        swap_endian(&sgi_header.ysize);
        swap_endian(&sgi_header.zsize);
        swap_endian(&sgi_header.pixmin);
        swap_endian(&sgi_header.pixmax);
        swap_endian(&sgi_header.colormap);
    }

    char dummy[404] = {0};
    if (!fwrite(&sgi_header.magic) ||
        !fwrite(&sgi_header.storage) ||
        !fwrite(&sgi_header.bpc) ||
        !fwrite(&sgi_header.dimension) ||
        !fwrite(&sgi_header.xsize) ||
        !fwrite(&sgi_header.ysize) ||
        !fwrite(&sgi_header.zsize) ||
        !fwrite(&sgi_header.pixmin) ||
        !fwrite(&sgi_header.pixmax) ||
        !fwrite(&sgi_header.dummy) ||
        !fwrite(sgi_header.imagename, 1, 80) ||
        !fwrite(&sgi_header.colormap) ||
        !fwrite(dummy, 404, 1)) {
        error ("Error writing to \"%s\"", m_filename);
        return false;
    }
    return true;
}

OIIO_PLUGIN_NAMESPACE_END

