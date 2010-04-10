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

#include <boost/algorithm/string/predicate.hpp>
using boost::algorithm::iequals;
#include "sgi_pvt.h"



// Obligatory material to make this a recognizeable imageio plugin
extern "C" {
    DLLEXPORT ImageOutput *sgi_output_imageio_create () {
        return new SgiOutput;
    }
    DLLEXPORT const char *sgi_output_extensions[] = {
        "sgi", "rgb", "rgba", "bw", "int", "inta", NULL
    };
};



bool
SgiOutput::open (const std::string &name, const ImageSpec &spec, bool append)
{
    // saving 'name' and 'spec' for later use
    m_filename = name;
    m_spec = spec;

    m_fd = fopen (m_filename.c_str (), "wb");
    if (!m_fd) {
        error ("Unable to open file \"%s\"", m_filename.c_str ());
        return false;
    }

    m_spec.set_format (TypeDesc::UINT8);

    create_and_write_header();

    return true;
}



bool
SgiOutput::supports (const std::string &feature) const
{
    if (iequals (feature, "random_access"))
        return true;
    if (iequals (feature, "rewrite"))
        return true;
    return false;
}



bool
SgiOutput::write_scanline (int y, int z, TypeDesc format, const void *data,
                           stride_t xstride)
{
    y = m_spec.height - y - 1;
    data = to_native_scanline (format, data, xstride, m_scratch);
    std::vector<unsigned char> scanline(m_spec.width * m_spec.nchannels);
    memcpy (&scanline[0], data, m_spec.width * m_spec.nchannels);

    std::vector<unsigned char> first_channel (m_spec.width);
    std::vector<unsigned char> second_channel (m_spec.width);
    std::vector<unsigned char> third_channel (m_spec.width);
    std::vector<unsigned char> fourth_channel (m_spec.width);
    for (int i = 0, j = 0; i < m_spec.width * m_spec.nchannels; i += m_spec.nchannels, j++) {
        first_channel[j] = scanline[i];
        if (m_spec.nchannels >= 3) {
            second_channel[j] = scanline[i+1];
            third_channel[j] = scanline[i+2];
            if (m_spec.nchannels == 4)
                fourth_channel[j] = scanline[i+3];
        }
    }

    // In SGI format all channels are saved to file separately: firsty all
    // channel 1 scanlines are saved, then all channel2 scanlines are saved
    // and so on.
    long scanline_off_chan1 = sgi_pvt::SGI_HEADER_LEN + y * m_spec.width;
    fseek (m_fd, scanline_off_chan1, SEEK_SET);
    fwrite (&first_channel[0], 1, first_channel.size(), m_fd);

    if (m_spec.nchannels >= 3) {
        long scanline_off_chan2 = sgi_pvt::SGI_HEADER_LEN + (m_spec.height + y)
                                  * m_spec.width;
        fseek (m_fd, scanline_off_chan2, SEEK_SET);
        fwrite (&second_channel[0], 1, second_channel.size(), m_fd);

        long scanline_off_chan3 = sgi_pvt::SGI_HEADER_LEN + (2 * m_spec.height + y)
                                  * m_spec.width;
        fseek (m_fd, scanline_off_chan3, SEEK_SET);
        fwrite (&third_channel[0], 1, third_channel.size(), m_fd);

        if (m_spec.nchannels == 4) {
            long scanline_off_chan4 = sgi_pvt::SGI_HEADER_LEN + (3 * m_spec.height + y)
                                      * m_spec.width;
            fseek (m_fd, scanline_off_chan4, SEEK_SET);
            fwrite (&fourth_channel[0], 1, fourth_channel.size(), m_fd);
        }
    }
    return true;    
}



bool
SgiOutput::close ()
{
    if (m_fd)
        fclose (m_fd);
    init ();
    return true;
}



void
SgiOutput::create_and_write_header()
{
    sgi_pvt::SgiHeader sgi_header;
    sgi_header.magic = sgi_pvt::SGI_MAGIC;
    sgi_header.storage = sgi_pvt::VERBATIM;
    sgi_header.bpc = 1;

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
    sgi_header.pixmax = 255;
    sgi_header.dummy = 0;

    ImageIOParameter *ip = m_spec.find_attribute ("ImageDescription",
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

    fwrite(&sgi_header.magic, sizeof(sgi_header.magic), 1, m_fd);
    fwrite(&sgi_header.storage, sizeof(sgi_header.storage), 1, m_fd);
    fwrite(&sgi_header.bpc, sizeof(sgi_header.bpc), 1, m_fd);
    fwrite(&sgi_header.dimension, sizeof(sgi_header.dimension), 1, m_fd);
    fwrite(&sgi_header.xsize, sizeof(sgi_header.xsize), 1, m_fd);
    fwrite(&sgi_header.ysize, sizeof(sgi_header.ysize), 1, m_fd);
    fwrite(&sgi_header.zsize, sizeof(sgi_header.zsize), 1, m_fd);
    fwrite(&sgi_header.pixmin, sizeof(sgi_header.pixmin), 1, m_fd);
    fwrite(&sgi_header.pixmax, sizeof(sgi_header.pixmax), 1, m_fd);
    fwrite(&sgi_header.dummy, sizeof(sgi_header.dummy), 1, m_fd);
    fwrite(sgi_header.imagename, sizeof(sgi_header.imagename), 1, m_fd);
    fwrite(&sgi_header.colormap, sizeof(sgi_header.colormap), 1, m_fd);
    char dummy[404] = {0};
    fwrite(dummy, 404, 1, m_fd);
}
